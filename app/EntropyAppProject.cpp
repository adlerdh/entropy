#include "EntropyApp.h"

#include "image/ImageUtility.h"
#include "layout/LayoutFileSerialization.h"
#include "logic/app/LoadingStatusItems.h"
#include "logic/app/ProjectSnapshotComparison.h"
#include "logic/app/ProjectSnapshotSettings.h"
#include "logic/app/WindowTitleStatus.h"
#include "logic/annotation/Annotation.h"
#include "logic/annotation/LandmarkGroup.h"
#include "logic/serialization/ProjectSerialization.h"
#include "ui/NativeFileDialogs.h"

#include <spdlog/fmt/std.h>
#include <spdlog/spdlog.h>

#include <cmath>
#include <utility>

namespace fs = std::filesystem;

namespace
{
fs::path projectSavePath(fs::path fileName)
{
  if (fileName.extension().empty()) {
    fileName += ".json";
  }
  return fileName;
}
fs::path resolvePathAgainstBase(const fs::path& path, const fs::path& basePath)
{
  if (path.is_absolute()) {
    return path;
  }
  return (basePath / path).lexically_normal();
}
bool saveCurrentLayoutsForProject(AppData& appData, const fs::path& layoutsFileName)
{
  layout::LayoutFile layoutFile{
    .m_currentLayoutIndex = appData.windowData().currentLayoutIndex(),
    .m_layouts = appData.windowData().createLayoutPresets(appData.imageUidsOrdered())};
  return layout::save(layoutFile, layoutsFileName);
}
constexpr uint64_t LargeImageWarningBytes = 2ull * 1024ull * 1024ull * 1024ull;

bool shouldPromptForLargeImage(const ImageHeader& header)
{
  return header.memoryImageSizeInBytes() >= LargeImageWarningBytes;
}
std::size_t numSerializedImages(const serialize::EntropyProject& project)
{
  return 1 + project.m_additionalImages.size();
}
serialize::Image* serializedImageAt(serialize::EntropyProject& project, std::size_t index)
{
  if (0 == index) {
    return &project.m_referenceImage;
  }

  const std::size_t additionalIndex = index - 1;
  if (additionalIndex < project.m_additionalImages.size()) {
    return &project.m_additionalImages[additionalIndex];
  }

  return nullptr;
}
void eraseSerializedImageAt(serialize::EntropyProject& project, std::size_t index)
{
  if (0 == index) {
    return;
  }

  const std::size_t additionalIndex = index - 1;
  if (additionalIndex < project.m_additionalImages.size()) {
    project.m_additionalImages.erase(project.m_additionalImages.begin() + static_cast<std::ptrdiff_t>(additionalIndex));
  }
}
bool isApproximatelyIdentity(const glm::mat4& matrix)
{
  constexpr float epsilon = 1.0e-5f;
  const glm::mat4 identity{1.0f};

  for (int c = 0; c < 4; ++c) {
    for (int r = 0; r < 4; ++r) {
      if (std::abs(matrix[c][r] - identity[c][r]) > epsilon) {
        return false;
      }
    }
  }

  return true;
}
} // namespace

serialize::EntropyProject EntropyApp::createProjectSnapshot() const
{
  serialize::EntropyProject project;
  const auto imageUids = m_data.imageUidsOrdered();

  if (imageUids.empty()) {
    return project;
  }

  const uuids::uuid referenceImageUid = m_data.refImageUid().value_or(imageUids.front());
  project.m_referenceImage = createImageSnapshot(referenceImageUid);

  for (const auto& imageUid : imageUids) {
    if (imageUid == referenceImageUid) {
      continue;
    }

    project.m_additionalImages.emplace_back(createImageSnapshot(imageUid));
  }

  project.m_layouts = m_data.windowData().createProjectLayoutSnapshots(imageUids);
  project.m_currentLayoutIndex = m_data.windowData().currentLayoutIndex();
  project.m_layoutsFileName = m_data.project().m_layoutsFileName;
  project.m_interface = project_snapshot::interfaceSettings(m_data);

  return project;
}

serialize::Image EntropyApp::createImageSnapshot(const uuids::uuid& imageUid) const
{
  serialize::Image serializedImage;
  const Image* image = m_data.image(imageUid);

  if (!image) {
    spdlog::warn("Cannot serialize missing image {}", imageUid);
    return serializedImage;
  }

  serializedImage.m_imageFileName = image->header().fileName();
  if (const auto sourceIt = m_dicomSourcesByImageUid.find(imageUid); sourceIt != m_dicomSourcesByImageUid.end()) {
    serializedImage.m_dicomSource = sourceIt->second;
  }
  serializedImage.m_affineTxFileName = image->transformations().get_affine_T_subject_fileName();
  if (
    image->transformations().get_enable_worldDef_T_affine() &&
    !isApproximatelyIdentity(image->transformations().get_worldDef_T_affine()))
  {
    serializedImage.m_worldDefTx = image->transformations().get_worldDef_T_affine();
  }
  serializedImage.m_settings = project_snapshot::imageSettings(*image);

  const auto defUids = m_data.imageToDefUids(imageUid);
  if (!defUids.empty()) {
    const Image* def = m_data.def(defUids.front());
    if (def && def->header().existsOnDisk() && !def->header().fileName().empty()) {
      serializedImage.m_deformationFileName = def->header().fileName();
    }

    if (defUids.size() > 1) {
      spdlog::warn(
        "Image {} has {} deformation fields, but project files currently save only the first one",
        imageUid,
        defUids.size());
    }
  }

  for (const auto& segUid : m_data.imageToSegUids(imageUid)) {
    const Image* seg = m_data.seg(segUid);
    if (!seg) {
      spdlog::warn("Cannot serialize missing segmentation {} for image {}", segUid, imageUid);
      continue;
    }

    if (!seg->header().existsOnDisk() || seg->header().fileName().empty()) {
      spdlog::debug("Skipping unsaved segmentation {} for image {}; it has no file-backed path", segUid, imageUid);
      continue;
    }

    serialize::Segmentation serializedSeg;
    serializedSeg.m_segFileName = seg->header().fileName();
    serializedSeg.m_settings = project_snapshot::segmentationSettings(*seg);
    serializedImage.m_segmentations.emplace_back(std::move(serializedSeg));
  }

  for (const auto& lmUid : m_data.imageToLandmarkGroupUids(imageUid)) {
    const LandmarkGroup* lmGroup = m_data.landmarkGroup(lmUid);
    if (!lmGroup) {
      spdlog::warn("Cannot serialize missing landmark group {} for image {}", lmUid, imageUid);
      continue;
    }

    if (lmGroup->getFileName().empty()) {
      spdlog::warn("Skipping unsaved landmark group {} for image {}; it has no file name", lmUid, imageUid);
      continue;
    }

    serialize::LandmarkGroup serializedLandmarks;
    serializedLandmarks.m_csvFileName = lmGroup->getFileName().string();
    serializedLandmarks.m_inVoxelSpace = lmGroup->getInVoxelSpace();
    serializedImage.m_landmarkGroups.emplace_back(std::move(serializedLandmarks));
  }

  for (const auto& annotationUid : m_data.annotationsForImage(imageUid)) {
    const Annotation* annotation = m_data.annotation(annotationUid);
    if (!annotation) {
      spdlog::warn("Cannot serialize missing annotation {} for image {}", annotationUid, imageUid);
      continue;
    }

    if (annotation->getFileName().empty()) {
      spdlog::warn("Skipping unsaved annotation {} for image {}; it has no file name", annotationUid, imageUid);
      continue;
    }

    if (serializedImage.m_annotationsFileName && *serializedImage.m_annotationsFileName != annotation->getFileName()) {
      spdlog::warn(
        "Image {} has annotations from multiple files; project files currently save only {}",
        imageUid,
        *serializedImage.m_annotationsFileName);
      continue;
    }

    serializedImage.m_annotationsFileName = annotation->getFileName();
  }

  return serializedImage;
}

bool EntropyApp::hasUnsavedAnnotations() const
{
  for (const auto& imageUid : m_data.imageUidsOrdered()) {
    for (const auto& annotationUid : m_data.annotationsForImage(imageUid)) {
      const Annotation* annotation = m_data.annotation(annotationUid);
      if (annotation && (annotation->isDirty() || annotation->getFileName().empty())) {
        return true;
      }
    }
  }

  return false;
}

bool EntropyApp::projectHasUnsavedChanges() const
{
  if (ProjectLoadState::Loaded != m_data.state().projectLoadState() || 0 == m_data.numImages()) {
    return false;
  }

  if (hasUnsavedAnnotations()) {
    return true;
  }

  if (!m_data.projectFileName()) {
    return true;
  }

  if (!m_savedProjectSnapshot) {
    return true;
  }

  return !project_snapshot::equivalent(createProjectSnapshot(), *m_savedProjectSnapshot);
}

bool EntropyApp::saveAnnotationsForImage(const uuids::uuid& imageUid, const fs::path& fileName)
{
  nlohmann::json annotationsJson;

  for (const auto& annotationUid : m_data.annotationsForImage(imageUid)) {
    const Annotation* annotation = m_data.annotation(annotationUid);
    if (annotation) {
      serialize::appendAnnotationToJson(*annotation, annotationsJson);
    }
  }

  if (!serialize::saveToJsonFile(annotationsJson, fileName)) {
    spdlog::error("Could not save annotations for image {} to {}", imageUid, fileName);
    return false;
  }

  for (const auto& annotationUid : m_data.annotationsForImage(imageUid)) {
    Annotation* annotation = m_data.annotation(annotationUid);
    if (annotation) {
      annotation->setFileName(fileName);
      annotation->markClean();
    }
  }

  spdlog::info("Saved annotations for image {} to JSON file {}", imageUid, fileName);
  return true;
}

bool EntropyApp::saveDirtyAnnotationsWithDialogs()
{
  for (const auto& imageUid : m_data.imageUidsOrdered()) {
    bool needsSave = false;
    std::optional<fs::path> existingFileName = std::nullopt;

    for (const auto& annotationUid : m_data.annotationsForImage(imageUid)) {
      const Annotation* annotation = m_data.annotation(annotationUid);
      if (!annotation) {
        continue;
      }

      if (!annotation->getFileName().empty() && !existingFileName) {
        existingFileName = annotation->getFileName();
      }

      if (annotation->isDirty() || annotation->getFileName().empty()) {
        needsSave = true;
      }
    }

    if (!needsSave) {
      continue;
    }

    fs::path annotationFileName;
    if (existingFileName) {
      annotationFileName = *existingFileName;
    }
    else {
      const Image* image = m_data.image(imageUid);
      const fs::path defaultDirectory = image ? image->header().fileName().parent_path() : fs::path{};
      const std::string defaultName =
        image ? (image->header().fileName().stem().string() + "_annotations.json") : std::string{"annotations.json"};
      const auto selectedFile =
        native_dialog::saveFile(native_dialog::annotationFilters(), defaultDirectory, defaultName);
      if (!selectedFile) {
        return false;
      }
      annotationFileName = *selectedFile;
    }

    if (!saveAnnotationsForImage(imageUid, annotationFileName)) {
      return false;
    }
  }

  return true;
}

void EntropyApp::markProjectSavedSnapshot()
{
  m_savedProjectSnapshot = createProjectSnapshot();
  m_data.setProject(*m_savedProjectSnapshot);
}

std::string EntropyApp::windowTitleStatus() const
{
  return window_title::status(m_data.projectFileName(), m_data.getAllImageDisplayNames(), projectHasUnsavedChanges());
}

void EntropyApp::updateWindowTitleStatus()
{
  m_glfw.setWindowTitleStatus(windowTitleStatus());
}

bool EntropyApp::saveProject()
{
  if (!saveDirtyAnnotationsWithDialogs()) {
    return false;
  }

  if (!m_data.projectFileName()) {
    spdlog::warn("Cannot save project because it has not been saved to a file yet");
    return false;
  }

  return saveProjectAs(*m_data.projectFileName());
}

bool EntropyApp::saveProjectAs(const fs::path& fileName)
{
  if (!saveDirtyAnnotationsWithDialogs()) {
    return false;
  }

  const fs::path normalizedFileName = projectSavePath(fileName);
  serialize::EntropyProject project = createProjectSnapshot();

  if (project.m_referenceImage.m_imageFileName.empty()) {
    spdlog::error("Cannot save project without a reference image");
    return false;
  }

  if (project.m_layoutsFileName) {
    const fs::path projectDirectory =
      normalizedFileName.parent_path().empty() ? fs::current_path() : normalizedFileName.parent_path();
    const fs::path layoutsFileName = resolvePathAgainstBase(*project.m_layoutsFileName, projectDirectory);
    if (!saveCurrentLayoutsForProject(m_data, layoutsFileName)) {
      spdlog::error("Could not save referenced layouts file {}", layoutsFileName);
      return false;
    }
    project.m_layoutsFileName = layoutsFileName;
  }

  if (!serialize::save(project, normalizedFileName)) {
    spdlog::error("Could not save project file {}", normalizedFileName);
    return false;
  }

  m_data.setProject(project);
  m_savedProjectSnapshot = project;
  m_data.setProjectFileName(normalizedFileName);
  updateWindowTitleStatus();
  m_glfw.postEmptyEvent();
  return true;
}

void EntropyApp::loadLayoutsFile(const fs::path& fileName)
{
  if (ProjectLoadState::Loaded != m_data.state().projectLoadState()) {
    m_pendingLayoutsFile = fileName;
    return;
  }

  layout::LayoutFile layoutFile;
  if (!layout::open(layoutFile, fileName)) {
    return;
  }

  if (layoutFile.m_layouts.empty()) {
    spdlog::warn("Layout file {} contains no layouts; using a Three-up layout", fileName);
  }

  if (!m_data.windowData().applyLayoutPresets(m_data, layoutFile.m_layouts, layoutFile.m_currentLayoutIndex)) {
    spdlog::error("Could not apply layout file {}", fileName);
    return;
  }

  std::error_code error;
  fs::path layoutsFileName = fs::canonical(fileName, error);
  if (error) {
    layoutsFileName = fs::absolute(fileName);
  }

  serialize::EntropyProject project = createProjectSnapshot();
  project.m_layoutsFileName = layoutsFileName;
  m_data.setProject(std::move(project));
  m_glfw.postEmptyEvent();
  spdlog::info("Loaded layouts from {}", fileName);
}

bool EntropyApp::saveLayoutsFile(const fs::path& fileName)
{
  if (ProjectLoadState::Loaded != m_data.state().projectLoadState()) {
    spdlog::warn("Cannot save layouts because no project is loaded");
    return false;
  }

  const bool saved = saveCurrentLayoutsForProject(m_data, fileName);
  if (saved) {
    serialize::EntropyProject project = createProjectSnapshot();
    project.m_layoutsFileName = fs::absolute(fileName);
    m_data.setProject(std::move(project));
    spdlog::info("Saved layouts to {}", fileName);
  }
  return saved;
}
void EntropyApp::loadProjectFile(const fs::path& fileName)
{
  if (fileName.empty()) {
    return;
  }

  m_pendingProjectReplacementPaths = {fileName};
  if (requestProjectReplacement(GuiData::UnsavedProjectAction::OpenProject)) {
    return;
  }
  clearPendingProjectReplacement();
  performLoadProjectFile(fileName);
}

void EntropyApp::performLoadProjectFile(const fs::path& fileName)
{
  serialize::EntropyProject project;

  if (!serialize::open(project, fileName)) {
    spdlog::error("Could not open project file {}", fileName);
    if (ProjectLoadState::Loaded != m_data.state().projectLoadState()) {
      m_data.state().setProjectLoadState(ProjectLoadState::Failed);
    }
    m_glfw.postEmptyEvent();
    return;
  }

  if (ProjectLoadState::Loaded == m_data.state().projectLoadState() && m_data.refImageUid()) {
    closeProject();
  }

  m_pendingLargeImageLoadContext = LargeImageLoadContext::Project;
  m_pendingLargeProject = std::move(project);
  m_pendingLargeProjectFileName = fileName;
  m_pendingLargeProjectImageIndex = 0;
  continueLargeImageProjectPreflight();
}

bool EntropyApp::requestProjectReplacement(GuiData::UnsavedProjectAction action)
{
  if (!projectHasUnsavedChanges()) {
    return false;
  }

  m_data.guiData().m_pendingUnsavedProjectAction = action;
  m_data.guiData().m_showUnsavedProjectPopup = true;
  m_glfw.postEmptyEvent();
  return true;
}

void EntropyApp::clearPendingProjectReplacement()
{
  m_pendingProjectReplacementPaths.clear();
}

void EntropyApp::beginLoadProject(serialize::EntropyProject project, std::optional<fs::path> projectFileName)
{
  closeProject();
  m_data.setProject(std::move(project));
  m_data.setProjectFileName(std::move(projectFileName));
  project_snapshot::applyInterfaceSettings(m_data, m_data.project().m_interface);

  startAsyncImageLoad(
    "Loading project...",
    [this]() { return loadProject(m_data.project()); },
    [this]() {
      m_data.clearProjectData();
      m_data.state().setProjectLoadState(ProjectLoadState::Failed);
      m_data.state().setAnimating(false);
      hideLoadingStatus();
      m_glfw.setEventProcessingMode(EventProcessingMode::Wait);
    },
    true,
    loading_status::projectItems(m_data.project()));
}

void EntropyApp::continueLargeImageProjectPreflight()
{
  if (!m_pendingLargeProject) {
    return;
  }

  while (m_pendingLargeProjectImageIndex < numSerializedImages(*m_pendingLargeProject)) {
    serialize::Image* image = serializedImageAt(*m_pendingLargeProject, m_pendingLargeProjectImageIndex);
    if (!image) {
      m_pendingLargeProjectImageIndex++;
      continue;
    }

    const auto header = readImageHeaderOnly(
      image->m_imageFileName,
      Image::ImageRepresentation::Image,
      Image::MultiComponentBufferType::SeparateImages);

    if (!header) {
      if (0 == m_pendingLargeProjectImageIndex) {
        spdlog::error("Could not read reference image header from {}; cancelling project load", image->m_imageFileName);
        m_pendingLargeImageLoadContext = LargeImageLoadContext::None;
        m_pendingLargeProject = std::nullopt;
        m_pendingLargeProjectFileName = std::nullopt;
        m_pendingLargeProjectImageIndex = 0;
        if (ProjectLoadState::Loaded != m_data.state().projectLoadState()) {
          m_data.state().setProjectLoadState(ProjectLoadState::Failed);
        }
        m_glfw.postEmptyEvent();
        return;
      }

      spdlog::error("Could not read image header from {}; skipping it", image->m_imageFileName);
      eraseSerializedImageAt(*m_pendingLargeProject, m_pendingLargeProjectImageIndex);
      continue;
    }

    if (shouldPromptForLargeImage(*header)) {
      spdlog::warn(
        "Image {} is large: estimated in-memory size is {:.2f} GiB",
        image->m_imageFileName,
        static_cast<double>(header->memoryImageSizeInBytes()) / (1024.0 * 1024.0 * 1024.0));

      m_pendingLargeImageLoadContext = LargeImageLoadContext::Project;
      m_data.guiData().m_pendingLargeImageLoadPrompt =
        GuiData::LargeImageLoadPrompt{image->m_imageFileName, *header, true, 0 != m_pendingLargeProjectImageIndex};
      m_data.guiData().m_showLargeImageLoadPrompt = true;
      m_glfw.postEmptyEvent();
      return;
    }

    m_pendingLargeProjectImageIndex++;
  }

  serialize::EntropyProject project = std::move(*m_pendingLargeProject);
  std::optional<fs::path> projectFileName = std::move(m_pendingLargeProjectFileName);
  m_pendingLargeImageLoadContext = LargeImageLoadContext::None;
  m_pendingLargeProject = std::nullopt;
  m_pendingLargeProjectFileName = std::nullopt;
  m_pendingLargeProjectImageIndex = 0;
  beginLoadProject(std::move(project), std::move(projectFileName));
}

void EntropyApp::handleLargeImageLoadDecision(GuiData::LargeImageLoadDecision decision)
{
  switch (m_pendingLargeImageLoadContext) {
    case LargeImageLoadContext::AddImage: {
      const std::optional<fs::path> fileName = m_pendingLargeAddImageFile;
      m_pendingLargeImageLoadContext = LargeImageLoadContext::None;
      m_pendingLargeAddImageFile = std::nullopt;

      if (GuiData::LargeImageLoadDecision::LoadOriginal == decision && fileName) {
        m_data.guiData().m_bypassNextImageLoadPreflight = true;
        addImageFile(*fileName);
      }
      break;
    }
    case LargeImageLoadContext::Project: {
      if (!m_pendingLargeProject) {
        m_pendingLargeImageLoadContext = LargeImageLoadContext::None;
        break;
      }

      if (GuiData::LargeImageLoadDecision::CancelProject == decision) {
        spdlog::info("Cancelled project load during large-image preflight");
        m_pendingLargeImageLoadContext = LargeImageLoadContext::None;
        m_pendingLargeProject = std::nullopt;
        m_pendingLargeProjectFileName = std::nullopt;
        m_pendingLargeProjectImageIndex = 0;
        m_glfw.postEmptyEvent();
        break;
      }

      if (GuiData::LargeImageLoadDecision::SkipImage == decision) {
        if (0 == m_pendingLargeProjectImageIndex) {
          spdlog::info("Cannot skip the reference image; cancelling project load");
          m_pendingLargeImageLoadContext = LargeImageLoadContext::None;
          m_pendingLargeProject = std::nullopt;
          m_pendingLargeProjectFileName = std::nullopt;
          m_pendingLargeProjectImageIndex = 0;
          m_glfw.postEmptyEvent();
          break;
        }

        serialize::Image* image = serializedImageAt(*m_pendingLargeProject, m_pendingLargeProjectImageIndex);
        if (image) {
          spdlog::info("Skipping large image {} during project load", image->m_imageFileName);
        }
        eraseSerializedImageAt(*m_pendingLargeProject, m_pendingLargeProjectImageIndex);
      }
      else {
        m_pendingLargeProjectImageIndex++;
      }

      continueLargeImageProjectPreflight();
      break;
    }
    case LargeImageLoadContext::None:
      break;
  }
}

void EntropyApp::requestCloseProject()
{
  if (projectHasUnsavedChanges()) {
    m_data.guiData().m_pendingUnsavedProjectAction = GuiData::UnsavedProjectAction::CloseProject;
    m_data.guiData().m_showUnsavedProjectPopup = true;
    m_glfw.postEmptyEvent();
    return;
  }

  closeProject();
}

void EntropyApp::requestQuitApp()
{
  if (projectHasUnsavedChanges()) {
    m_data.guiData().m_pendingUnsavedProjectAction = GuiData::UnsavedProjectAction::QuitApp;
    m_data.guiData().m_showUnsavedProjectPopup = true;
    m_glfw.postEmptyEvent();
    return;
  }

  m_data.guiData().m_showConfirmCloseAppPopup = true;
  m_glfw.postEmptyEvent();
}

void EntropyApp::quitAppWithoutPrompt()
{
  m_data.state().setQuitApp(true);
  m_glfw.postEmptyEvent();
}

void EntropyApp::continueAfterUnsavedProjectPrompt()
{
  const GuiData::UnsavedProjectAction action = m_data.guiData().m_pendingUnsavedProjectAction;
  const std::vector<fs::path> pendingPaths = m_pendingProjectReplacementPaths;
  clearPendingProjectReplacement();

  switch (action) {
    case GuiData::UnsavedProjectAction::CloseProject:
      closeProject();
      return;
    case GuiData::UnsavedProjectAction::OpenImages:
      closeProject();
      performLoadImageFiles(pendingPaths);
      return;
    case GuiData::UnsavedProjectAction::OpenDicomSeries:
      closeProject();
      performOpenDicomSeriesFolders(pendingPaths);
      return;
    case GuiData::UnsavedProjectAction::OpenProject:
      if (!pendingPaths.empty()) {
        performLoadProjectFile(pendingPaths.front());
      }
      return;
    case GuiData::UnsavedProjectAction::QuitApp:
      quitAppWithoutPrompt();
      return;
  }
}

void EntropyApp::closeProject()
{
  m_imageLoadCancelled = true;

  if (m_futureLoadProject.valid()) {
    m_futureLoadProject.wait();
    m_futureLoadProject = {};
  }
  if (m_futureDiscoverDicom.valid()) {
    m_futureDiscoverDicom.wait();
    m_futureDiscoverDicom = {};
  }

  m_imageLoadCancelled = false;
  m_imagesReady = false;
  m_imageLoadFailed = false;
  m_preserveLayoutsOnImagesReady = false;
  m_pendingAddedImageUids.clear();
  m_pendingLayoutsFile = std::nullopt;
  m_pendingLargeImageLoadContext = LargeImageLoadContext::None;
  m_pendingLargeAddImageFile = std::nullopt;
  m_pendingLargeProject = std::nullopt;
  m_pendingLargeProjectFileName = std::nullopt;
  m_pendingLargeProjectImageIndex = 0;
  m_savedProjectSnapshot = std::nullopt;
  hideLoadingStatus();
  clearPendingProjectReplacement();
  m_data.guiData().m_pendingLargeImageLoadPrompt = std::nullopt;
  m_data.guiData().m_dicomSeriesScanInProgress = false;
  m_data.guiData().m_pendingDicomScanRoot = fs::path{};
  m_data.guiData().m_pendingDicomSeriesSelectionPrompt = std::nullopt;
  m_data.guiData().m_showDicomSeriesSelectionPopup = false;
  m_data.guiData().m_showUnsavedProjectPopup = false;
  m_data.guiData().m_showConfirmCloseAppPopup = false;
  m_data.guiData().m_showLargeImageLoadPrompt = false;

  auto& renderData = m_data.renderData();
  renderData.m_imageTextures.clear();
  renderData.m_distanceMapTextures.clear();
  renderData.m_segTextures.clear();
  renderData.m_labelBufferTextures.clear();
  renderData.m_colormapTextures.clear();
  renderData.m_uniforms.clear();

  m_data.clearProjectData();
  m_glfw.setWindowTitleStatus("");
  m_glfw.setEventProcessingMode(EventProcessingMode::Wait);
  m_glfw.postEmptyEvent();
}
