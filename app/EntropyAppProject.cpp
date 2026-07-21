#include "EntropyApp.h"

#include "image/ImageUtility.h"
#include "logic/app/AppPaths.h"
#include "layout/LayoutFileSerialization.h"
#include "logic/app/LoadingStatusItems.h"
#include "logic/app/ProjectSnapshotComparison.h"
#include "logic/app/ProjectSnapshotSettings.h"
#include "logic/app/UserPreferences.h"
#include "logic/app/WindowTitleStatus.h"
#include "logic/annotation/Annotation.h"
#include "logic/annotation/LandmarkGroup.h"
#include "logic/annotation/SerializeAnnot.h"
#include "logic/serialization/ProjectSerialization.h"
#include "registration/Artifacts.h"
#include "ui/NativeFileDialogs.h"

#include <spdlog/fmt/std.h>
#include <spdlog/spdlog.h>

#include <cmath>
#include <set>
#include <string>
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

serialize::RegistrationResult registrationResultSnapshot(const registration::JobRecord& job)
{
  serialize::RegistrationResult result;
  result.m_backend = std::string{registration::label(job.spec.backend)};
  result.m_fixedImageUid = job.spec.fixedImage.uid;
  result.m_movingImageUid = job.spec.movingImage.uid;

  if (!job.manifest) {
    return result;
  }

  const registration::ResultManifest& manifest = *job.manifest;
  result.m_manifestFileName = registration::artifactPath(job.spec, registration::ArtifactRole::ResultManifest);
  if (!manifest.warpedImage.empty()) {
    result.m_warpedImage = manifest.warpedImage;
  }
  if (!manifest.inverseWarp.empty()) {
    result.m_inverseWarpField = manifest.inverseWarp;
  }
  if (!manifest.forwardWarp.empty()) {
    result.m_forwardWarpField = manifest.forwardWarp;
  }
  if (!manifest.affineTransform.empty()) {
    result.m_affineTransform = manifest.affineTransform;
  }
  result.m_warpedSegmentations = manifest.warpedSegmentations;
  result.m_transformedSurfaces = manifest.transformedSurfaces;
  result.m_transformedLandmarks = manifest.transformedLandmarks;
  result.m_warnings = manifest.warnings;

  return result;
}

std::string registrationResultKey(const serialize::RegistrationResult& result)
{
  const std::string manifest = result.m_manifestFileName ? result.m_manifestFileName->string() : std::string{};
  return result.m_backend + '\n' + result.m_fixedImageUid + '\n' + result.m_movingImageUid + '\n' + manifest;
}

std::vector<serialize::RegistrationResult> registrationResultSnapshots(const AppData& data)
{
  std::vector<serialize::RegistrationResult> results = data.project().m_registrationResults;
  std::set<std::string> keys;
  for (const serialize::RegistrationResult& result : results) {
    keys.insert(registrationResultKey(result));
  }

  for (const registration::JobRecord& job : data.registrationJobs().jobs()) {
    if (job.status != registration::JobStatus::Completed || !job.manifest) {
      continue;
    }

    serialize::RegistrationResult result = registrationResultSnapshot(job);
    if (keys.insert(registrationResultKey(result)).second) {
      results.push_back(std::move(result));
    }
  }

  return results;
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
  project.m_interface = project_snapshot::interfaceSettings(m_data);
  project.m_view = project_snapshot::viewSettings(m_data);
  project.m_comparison = project_snapshot::comparisonSettings(m_data);
  project.m_raycasting = project_snapshot::raycastingSettings(m_data);
  project.m_intensityProjection = project_snapshot::intensityProjectionSettings(m_data);
  project.m_segmentationDisplay = project_snapshot::segmentationDisplaySettings(m_data);
  project.m_isosurfaces = project_snapshot::isosurfaceDisplaySettings(m_data);
  project.m_annotationDisplay = project_snapshot::annotationDisplaySettings(m_data);
  project.m_registrationResults = registrationResultSnapshots(m_data);

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
  serializedImage.m_spatialMetadata = image->header().userSpatialMetadata();
  if (const auto sourceIt = m_dicomSourcesByImageUid.find(imageUid); sourceIt != m_dicomSourcesByImageUid.end()) {
    serializedImage.m_dicomSource = sourceIt->second;
  }
  const auto& transformations = image->transformations();
  if (
    transformations.get_affine_T_subject_fileName() ||
    (transformations.get_enable_affine_T_subject() && !isApproximatelyIdentity(transformations.get_affine_T_subject())))
  {
    serializedImage.m_initialAffineMatrix = transformations.get_affine_T_subject();
  }

  if (
    transformations.get_enable_worldDef_T_affine() && !isApproximatelyIdentity(transformations.get_worldDef_T_affine()))
  {
    serializedImage.m_manualAffineMatrix = transformations.get_worldDef_T_affine();
  }
  serializedImage.m_settings = project_snapshot::imageSettings(*image);

  const auto defUids = m_data.imageToDefUids(imageUid);
  if (!defUids.empty()) {
    const auto activeInverseWarpUid = m_data.imageToActiveInverseWarpUid(imageUid);
    const Image* inverseWarp =
      activeInverseWarpUid ? m_data.warpField(*activeInverseWarpUid) : m_data.warpField(defUids.front());
    if (inverseWarp && inverseWarp->header().existsOnDisk() && !inverseWarp->header().fileName().empty()) {
      serializedImage.m_inverseWarpFieldPath = inverseWarp->header().fileName();
    }

    if (const auto inverseWarpReferenceUid = m_data.imageToActiveInverseWarpReferenceImageUid(imageUid)) {
      const Image* inverseWarpReferenceImage = m_data.image(*inverseWarpReferenceUid);
      if (
        inverseWarpReferenceImage && inverseWarpReferenceImage->header().existsOnDisk() &&
        !inverseWarpReferenceImage->header().fileName().empty())
      {
        serializedImage.m_inverseWarpReferenceImagePath = inverseWarpReferenceImage->header().fileName();
      }
    }

    if (const auto activeForwardWarpUid = m_data.imageToActiveForwardWarpUid(imageUid)) {
      const Image* forwardWarp = m_data.warpField(*activeForwardWarpUid);
      if (forwardWarp && forwardWarp->header().existsOnDisk() && !forwardWarp->header().fileName().empty()) {
        serializedImage.m_forwardWarpFieldPath = forwardWarp->header().fileName();
      }
    }

    if (defUids.size() > 1) {
      spdlog::warn(
        "Image {} has {} warp fields, but project files currently save only the active inverse and forward ones",
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

    serialize::LandmarkGroup serializedLandmarks;
    if (!lmGroup->getFileName().empty()) {
      serializedLandmarks.m_csvFileName = lmGroup->getFileName();
    }
    serializedLandmarks.m_inVoxelSpace = lmGroup->getInVoxelSpace();
    serializedLandmarks.m_name = lmGroup->getName();
    serializedLandmarks.m_visible = lmGroup->getVisibility();
    serializedLandmarks.m_opacity = lmGroup->getOpacity();
    serializedLandmarks.m_color = lmGroup->getColor();
    serializedLandmarks.m_colorOverride = lmGroup->getColorOverride();
    serializedLandmarks.m_textColor = lmGroup->getTextColor();
    serializedLandmarks.m_renderLandmarkIndices = lmGroup->getRenderLandmarkIndices();
    serializedLandmarks.m_renderLandmarkNames = lmGroup->getRenderLandmarkNames();
    serializedLandmarks.m_radiusFactor = lmGroup->getRadiusFactor();
    for (const auto& [index, point] : lmGroup->getPoints()) {
      serializedLandmarks.m_points.push_back(
        serialize::LandmarkPoint{.m_index = index, .m_position = point.getPosition(), .m_name = point.getName()});
    }
    serializedImage.m_landmarkGroups.emplace_back(std::move(serializedLandmarks));
  }

  for (const auto& annotationUid : m_data.annotationsForImage(imageUid)) {
    const Annotation* annotation = m_data.annotation(annotationUid);
    if (!annotation) {
      spdlog::warn("Cannot serialize missing annotation {} for image {}", annotationUid, imageUid);
      continue;
    }

    if (
      !annotation->getFileName().empty() && serializedImage.m_annotationsFileName &&
      *serializedImage.m_annotationsFileName != annotation->getFileName())
    {
      spdlog::warn(
        "Image {} has annotations from multiple files; project files currently reference only {}",
        imageUid,
        *serializedImage.m_annotationsFileName);
    }

    if (!annotation->getFileName().empty() && !serializedImage.m_annotationsFileName) {
      serializedImage.m_annotationsFileName = annotation->getFileName();
    }
    serializedImage.m_annotations.push_back(*annotation);
  }

  for (uint32_t component = 0; component < image->header().numComponentsPerPixel(); ++component) {
    for (const auto& isosurfaceUid : m_data.isosurfaceUids(imageUid, component)) {
      const Isosurface* surface = m_data.isosurface(imageUid, component, isosurfaceUid);
      if (!surface) {
        spdlog::warn("Cannot serialize missing isosurface {} for image {}", isosurfaceUid, imageUid);
        continue;
      }

      serialize::ImageIsosurface serializedSurface;
      serializedSurface.m_component = component;
      serializedSurface.m_surface = *surface;
      serializedImage.m_isosurfaces.emplace_back(std::move(serializedSurface));
    }
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
  std::vector<Annotation> annotations;

  for (const auto& annotationUid : m_data.annotationsForImage(imageUid)) {
    const Annotation* annotation = m_data.annotation(annotationUid);
    if (annotation) {
      annotations.push_back(*annotation);
    }
  }

  const nlohmann::json annotationsJson = annotationsToJson(annotations);

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

void EntropyApp::resetProjectSettings()
{
  if (ProjectLoadState::Loaded != m_data.state().projectLoadState()) {
    return;
  }

  project_snapshot::applyDefaultProjectSettings(m_data);
  m_rendering.updateMetricUniforms();
  m_data.setProject(createProjectSnapshot());
  updateWindowTitleStatus();
  spdlog::info("Reset project settings to defaults");
}

std::string EntropyApp::windowTitleStatus() const
{
  return window_title::status(m_data.projectFileName(), m_data.getAllImageDisplayNames(), projectHasUnsavedChanges());
}

void EntropyApp::updateWindowTitleStatus()
{
  m_glfw.setWindowTitleStatus(windowTitleStatus());
}

void EntropyApp::saveAppSettingsQuietly()
{
  if (m_data.guiData().m_appSettingsDirty) {
    return;
  }

  std::string error;
  const fs::path settingsFile = app_paths::userSettingsFile();
  if (!user_preferences::save(m_data.settings(), m_data.renderData(), m_data.guiData(), settingsFile, &error)) {
    spdlog::warn("Could not save recent file history to {}: {}", settingsFile, error);
    return;
  }
  user_preferences::markSavedAppSettingsState(m_data.settings(), m_data.renderData(), m_data.guiData());
}

void EntropyApp::recordRecentImageGroup(const std::vector<fs::path>& fileNames)
{
  m_data.settings().recordRecentImageGroup(fileNames);
  saveAppSettingsQuietly();
}

void EntropyApp::recordRecentDicomGroup(const std::vector<fs::path>& folderNames)
{
  m_data.settings().recordRecentDicomGroup(folderNames);
  saveAppSettingsQuietly();
}

void EntropyApp::recordRecentProjectFile(const fs::path& fileName)
{
  m_data.settings().recordRecentProjectFile(fileName);
  saveAppSettingsQuietly();
}

bool EntropyApp::saveProject()
{
  if (!m_data.projectFileName()) {
    spdlog::warn("Cannot save project because it has not been saved to a file yet");
    return false;
  }

  return saveProjectAs(*m_data.projectFileName());
}

bool EntropyApp::saveProjectAs(const fs::path& fileName)
{
  const fs::path normalizedFileName = projectSavePath(fileName);
  serialize::EntropyProject project = createProjectSnapshot();

  if (project.m_referenceImage.m_imageFileName.empty()) {
    spdlog::error("Cannot save project without a reference image");
    return false;
  }

  if (!serialize::save(project, normalizedFileName)) {
    spdlog::error("Could not save project file {}", normalizedFileName);
    return false;
  }

  for (const auto& imageUid : m_data.imageUidsOrdered()) {
    for (const auto& annotationUid : m_data.annotationsForImage(imageUid)) {
      if (Annotation* annotation = m_data.annotation(annotationUid)) {
        annotation->markClean();
      }
    }
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
    spdlog::warn("Layout file {} contains no layouts; using the default layout", fileName);
  }

  if (!m_data.windowData().applyLayoutPresets(m_data, layoutFile.m_layouts, layoutFile.m_currentLayoutIndex)) {
    spdlog::error("Could not apply layout file {}", fileName);
    return;
  }

  m_data.setProject(createProjectSnapshot());
  m_glfw.postEmptyEvent();
  spdlog::info("Imported layouts from {}", fileName);
}

bool EntropyApp::saveLayoutsFile(const fs::path& fileName)
{
  if (ProjectLoadState::Loaded != m_data.state().projectLoadState()) {
    spdlog::warn("Cannot save layouts because no project is loaded");
    return false;
  }

  const bool saved = saveCurrentLayoutsForProject(m_data, fileName);
  if (saved) {
    spdlog::info("Exported layouts to {}", fileName);
  }
  return saved;
}
void EntropyApp::loadProjectFile(const fs::path& fileName)
{
  if (fileName.empty()) {
    return;
  }

  spdlog::info("Opening project file {}", fileName);

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

  spdlog::info("Loading project file {}", fileName);

  if (!serialize::open(project, fileName)) {
    spdlog::error("Could not open project file {}", fileName);
    if (ProjectLoadState::Loaded != m_data.state().projectLoadState()) {
      m_data.state().setProjectLoadState(ProjectLoadState::Failed);
    }
    m_glfw.postEmptyEvent();
    return;
  }

  recordRecentProjectFile(fileName);

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
  if (projectFileName) {
    spdlog::info("Beginning project load from {}", *projectFileName);
  }
  else {
    spdlog::info("Beginning project load from image inputs");
  }
  m_data.setProject(std::move(project));
  m_data.setProjectFileName(std::move(projectFileName));
  project_snapshot::applyInterfaceSettings(m_data, m_data.project().m_interface);
  project_snapshot::applyViewSettings(m_data, m_data.project().m_view);
  project_snapshot::applyComparisonSettings(m_data, m_data.project().m_comparison);
  project_snapshot::applyRaycastingSettings(m_data, m_data.project().m_raycasting);
  project_snapshot::applyIntensityProjectionSettings(m_data, m_data.project().m_intensityProjection);
  project_snapshot::applySegmentationDisplaySettings(m_data, m_data.project().m_segmentationDisplay);
  project_snapshot::applyIsosurfaceDisplaySettings(m_data, m_data.project().m_isosurfaces);
  project_snapshot::applyAnnotationDisplaySettings(m_data, m_data.project().m_annotationDisplay);

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
  user_preferences::updateAppSettingsDirtyState(m_data.settings(), m_data.renderData(), m_data.guiData());

  if (projectHasUnsavedChanges()) {
    m_data.guiData().m_pendingUnsavedProjectAction = GuiData::UnsavedProjectAction::QuitApp;
    m_data.guiData().m_showUnsavedProjectPopup = true;
    m_glfw.postEmptyEvent();
    return;
  }

  if (m_data.guiData().m_appSettingsDirty) {
    m_data.guiData().m_showUnsavedAppSettingsPopup = true;
    m_glfw.postEmptyEvent();
    return;
  }

  const bool hasLoadedData = ProjectLoadState::Loaded == m_data.state().projectLoadState() && 0 < m_data.numImages();
  if (!hasLoadedData) {
    quitAppWithoutPrompt();
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
      if (m_data.guiData().m_appSettingsDirty) {
        m_data.guiData().m_showUnsavedAppSettingsPopup = true;
        m_glfw.postEmptyEvent();
        return;
      }
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
  m_data.guiData().m_showUnsavedAppSettingsPopup = false;
  m_data.guiData().m_showConfirmCloseAppPopup = false;
  m_data.guiData().m_showLargeImageLoadPrompt = false;

  auto& renderData = m_data.renderData();
  renderData.m_imageTextures.clear();
  renderData.m_imageTextureLayouts.clear();
  renderData.m_distanceMapTextures.clear();
  renderData.m_segTextures.clear();
  renderData.m_segTextureLayouts.clear();
  renderData.m_labelBufferTextures.clear();
  renderData.m_colormapTextures.clear();
  renderData.m_uniforms.clear();

  m_data.clearProjectData();
  m_glfw.setWindowTitleStatus("");
  m_glfw.setEventProcessingMode(EventProcessingMode::Wait);
  m_glfw.postEmptyEvent();
}
