#include "EntropyApp.h"
#include "logic/app/ProjectSnapshot.h"

#include "image/ImageUtility.h"
#include "image/ImageWriter.h"
#include "common/UuidUtility.h"
#include "ui/ExportJobService.h"
#include "logic/app/AppPaths.h"
#include "layout/LayoutFileSerialization.h"
#include "logic/app/LargeImagePolicy.h"
#include "logic/app/LoadingStatusItems.h"
#include "logic/app/ProjectImageSequence.h"
#include "logic/app/ProjectLayoutDelta.h"
#include "logic/app/ProjectSnapshotComparison.h"
#include "logic/app/ProjectSnapshotSettings.h"
#include "logic/app/UserPreferences.h"
#include "logic/app/WindowTitleStatus.h"
#include "logic/annotation/Annotation.h"
#include "logic/annotation/LandmarkGroup.h"
#include "logic/annotation/SerializeAnnot.h"
#include "logic/serialization/ProjectSerialization.h"
#include "mesh/MeshTypes.h"
#include "registration/Artifacts.h"
#include "ui/NativeFileDialogs.h"
#include "ui/dialogs/NativeMessageDialogs.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/color_space.hpp>

#include <spdlog/fmt/std.h>
#include <spdlog/spdlog.h>

#include <algorithm>
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
    .m_layouts = appData.windowData().createProjectLayoutSnapshots(appData.imageUidsOrdered())};
  return layout::save(layoutFile, layoutsFileName);
}

} // namespace

serialize::EntropyProject EntropyApp::createProjectSnapshot() const
{
  return project_snapshot::captureProject(m_data, m_dicomSourcesByImageUid, dicomNativeViewTypesByImage());
}

serialize::Image EntropyApp::createImageSnapshot(
  const uuids::uuid& imageUid,
  const std::optional<glm::vec3>& defaultBorderColor) const
{
  return project_snapshot::captureImage(m_data, m_dicomSourcesByImageUid, imageUid, defaultBorderColor);
}

bool EntropyApp::hasUnsavedAnnotations() const
{
  for (const auto& imageUid : m_data.imageUidsOrdered()) {
    for (const auto& annotationUid : m_data.annotationsForImage(imageUid)) {
      const Annotation* annotation = m_data.annotation(annotationUid);
      if (annotation && annotation->isDirty()) {
        return true;
      }
    }
  }

  return false;
}

bool EntropyApp::hasUnsavedSegmentations() const
{
  const auto& imageUids = m_data.imageUidsOrdered();
  return std::any_of(imageUids.begin(), imageUids.end(), [this](const auto& imageUid) {
    const auto& segmentationUids = m_data.imageToSegUids(imageUid);
    return std::any_of(segmentationUids.begin(), segmentationUids.end(), [this](const auto& segmentationUid) {
      return m_data.segmentationHasUnsavedVoxelChanges(segmentationUid);
    });
  });
}

bool EntropyApp::projectHasUnsavedChanges() const
{
  if (ProjectLoadState::Loaded != m_data.state().projectLoadState() || 0 == m_data.numImages()) {
    return false;
  }

  if (hasUnsavedAnnotations() || hasUnsavedSegmentations()) {
    return true;
  }
  for (const auto& imageUid : m_data.imageUidsOrdered()) {
    for (const auto& warpUid : m_data.imageToDefUids(imageUid)) {
      const Image* warp = m_data.warpField(warpUid);
      if (warp && (!warp->header().existsOnDisk() || warp->header().fileName().empty())) return true;
    }
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
  if (!user_preferences::save(
        m_data.settings(),
        m_data.applicationRenderPreferences(),
        user_preferences::precisionPreferencesFrom(m_data.guiData()),
        settingsFile,
        &error))
  {
    spdlog::warn("Could not save recent file history to {}: {}", settingsFile, error);
    return;
  }
  user_preferences::markSavedAppSettingsState(
    m_data.settings(),
    m_data.applicationRenderPreferences(),
    m_data.guiData());
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

void EntropyApp::beginPendingRecentDataLoad(recent_data::Kind kind, std::vector<fs::path> paths)
{
  m_pendingRecentDataLoad.begin(kind, std::move(paths));
}

void EntropyApp::commitPendingRecentDataLoad()
{
  const std::optional<recent_data::Entry> entry = m_pendingRecentDataLoad.takeCompleted();
  if (!entry) {
    return;
  }

  switch (entry->kind) {
    case recent_data::Kind::Images:
      recordRecentImageGroup(entry->paths);
      break;
    case recent_data::Kind::Dicom:
      recordRecentDicomGroup(entry->paths);
      break;
    case recent_data::Kind::Project:
      recordRecentProjectFile(entry->paths.front());
      break;
  }
}

void EntropyApp::clearPendingRecentDataLoad()
{
  m_pendingRecentDataLoad.cancel();
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
  if (hasUnsavedSegmentations()) {
    spdlog::error("Cannot save project while it contains segmentation voxel edits newer than their saved files");
    native_dialog::showMessageDialog(
      {"Unsaved Segmentation Edits",
       "The project cannot be saved yet.",
       "Export each edited segmentation, wait for its export to finish, then save the project again.",
       "OK",
       "",
       ""});
    return false;
  }

  const fs::path normalizedFileName = projectSavePath(fileName);
  const auto saved = project_snapshot::persistProject(
    m_data,
    normalizedFileName,
    m_dicomSourcesByImageUid,
    dicomNativeViewTypesByImage());

  if (!saved) return false;
  const auto& project = *saved;

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
  recordRecentProjectFile(normalizedFileName);
  updateWindowTitleStatus();
  GlfwWrapper::postEmptyEvent();
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
    reportInputLoadFailure("layout", fileName, "The layout file could not be read or parsed.");
    return;
  }

  if (layoutFile.m_layouts.empty()) {
    spdlog::warn("Layout file {} contains no layouts; using the default layout", fileName);
  }

  if (!m_data.windowData()
         .applyProjectLayoutSnapshots(layoutFile.m_layouts, m_data.imageUidsOrdered(), layoutFile.m_currentLayoutIndex))
  {
    spdlog::error("Could not apply layout file {}", fileName);
    reportInputLoadFailure(
      "layout",
      fileName,
      "The layout definitions are invalid or incompatible with the currently loaded images.");
    return;
  }

  if (m_data.renderSettings().m_synchronizeThreeDCameras) {
    m_data.windowData().synchronizeCurrentLayoutThreeDCameras();
  }

  m_data.setProject(createProjectSnapshot());
  GlfwWrapper::postEmptyEvent();
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

  spdlog::info("Requested project file {}", fileName);

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

  spdlog::info("Reading project file {}", fileName);

  if (!serialize::open(project, fileName)) {
    spdlog::error("Could not open project file {}", fileName);
    reportInputLoadFailure("project", fileName, "The project file could not be read or parsed.");

    if (ProjectLoadState::Loaded != m_data.state().projectLoadState()) {
      m_data.state().setProjectLoadState(ProjectLoadState::Failed);
    }

    GlfwWrapper::postEmptyEvent();
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
  GlfwWrapper::postEmptyEvent();
  return true;
}

void EntropyApp::clearPendingProjectReplacement()
{
  m_pendingProjectReplacementPaths.clear();
}

void EntropyApp::beginLoadProject(serialize::EntropyProject project, std::optional<fs::path> projectFileName)
{
  if (projectFileName) {
    beginPendingRecentDataLoad(recent_data::Kind::Project, {*projectFileName});
  }
  closeProject();
  if (projectFileName) {
    spdlog::info("Beginning project load from {}", *projectFileName);
  }
  else {
    spdlog::info("Beginning project load from image inputs");
  }

  m_data.setProject(std::move(project));
  m_data.setProjectFileName(std::move(projectFileName));

  project_snapshot::applySynchronizationSettings(m_data, m_data.project().m_synchronization);
  project_snapshot::applyViewSettings(m_data, m_data.project().m_view);
  project_snapshot::applyComparisonSettings(m_data, m_data.project().m_comparison);
  project_snapshot::applyThreeDRenderingSettings(m_data, m_data.project().m_threeDRendering);
  project_snapshot::applyRaycastingSettings(m_data, m_data.project().m_raycasting);
  project_snapshot::applyMeshRenderingSettings(m_data, m_data.project().m_meshRendering);
  project_snapshot::applyIntensityProjectionSettings(m_data, m_data.project().m_intensityProjection);
  project_snapshot::applySegmentationDisplaySettings(m_data, m_data.project().m_segmentationDisplay);
  project_snapshot::applyIsocontourDisplaySettings(m_data, m_data.project().m_isocontours);

  startAsyncImageLoad(
    "Loading project...",
    [this]() { return loadProject(m_data.project()); },
    [this]() {
      clearPendingRecentDataLoad();
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

  while (m_pendingLargeProjectImageIndex < project_image_sequence::size(*m_pendingLargeProject)) {
    serialize::Image* image = project_image_sequence::at(*m_pendingLargeProject, m_pendingLargeProjectImageIndex);
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
        reportInputLoadFailure("image", image->m_imageFileName, "The reference image header could not be read.");
        m_pendingLargeImageLoadContext = LargeImageLoadContext::None;
        m_pendingLargeProject = std::nullopt;
        m_pendingLargeProjectFileName = std::nullopt;
        m_pendingLargeProjectImageIndex = 0;
        clearPendingRecentDataLoad();
        if (ProjectLoadState::Loaded != m_data.state().projectLoadState()) {
          m_data.state().setProjectLoadState(ProjectLoadState::Failed);
        }
        GlfwWrapper::postEmptyEvent();
        return;
      }

      spdlog::error("Could not read image header from {}; skipping it", image->m_imageFileName);
      reportInputLoadFailure("image", image->m_imageFileName, "The image header could not be read.");
      project_image_sequence::erase(*m_pendingLargeProject, m_pendingLargeProjectImageIndex);
      continue;
    }

    if (large_image_policy::requiresConfirmation(header->memoryImageSizeInBytes())) {
      spdlog::warn(
        "Image {} is large: estimated in-memory size is {:.2f} GiB",
        image->m_imageFileName,
        static_cast<double>(header->memoryImageSizeInBytes()) / (1024.0 * 1024.0 * 1024.0));

      m_pendingLargeImageLoadContext = LargeImageLoadContext::Project;
      m_data.guiData().m_pendingLargeImageLoadPrompt =
        GuiData::LargeImageLoadPrompt{image->m_imageFileName, *header, true, 0 != m_pendingLargeProjectImageIndex};
      m_data.guiData().m_showLargeImageLoadPrompt = true;
      GlfwWrapper::postEmptyEvent();
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
        clearPendingRecentDataLoad();
        GlfwWrapper::postEmptyEvent();
        break;
      }

      if (GuiData::LargeImageLoadDecision::SkipImage == decision) {
        if (0 == m_pendingLargeProjectImageIndex) {
          spdlog::info("Cannot skip the reference image; cancelling project load");
          m_pendingLargeImageLoadContext = LargeImageLoadContext::None;
          m_pendingLargeProject = std::nullopt;
          m_pendingLargeProjectFileName = std::nullopt;
          m_pendingLargeProjectImageIndex = 0;
          clearPendingRecentDataLoad();
          GlfwWrapper::postEmptyEvent();
          break;
        }

        serialize::Image* image = project_image_sequence::at(*m_pendingLargeProject, m_pendingLargeProjectImageIndex);
        if (image) {
          spdlog::info("Skipping large image {} during project load", image->m_imageFileName);
        }
        project_image_sequence::erase(*m_pendingLargeProject, m_pendingLargeProjectImageIndex);
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
    GlfwWrapper::postEmptyEvent();
    return;
  }

  closeProject();
}

void EntropyApp::requestQuitApp()
{
  user_preferences::updateAppSettingsDirtyState(
    m_data.settings(),
    m_data.applicationRenderPreferences(),
    m_data.guiData());

  if (projectHasUnsavedChanges()) {
    m_data.guiData().m_pendingUnsavedProjectAction = GuiData::UnsavedProjectAction::QuitApp;
    m_data.guiData().m_showUnsavedProjectPopup = true;
    GlfwWrapper::postEmptyEvent();
    return;
  }

  if (m_data.guiData().m_appSettingsDirty) {
    m_data.guiData().m_showUnsavedAppSettingsPopup = true;
    GlfwWrapper::postEmptyEvent();
    return;
  }

  const bool hasLoadedData = ProjectLoadState::Loaded == m_data.state().projectLoadState() && 0 < m_data.numImages();
  if (!hasLoadedData) {
    quitAppWithoutPrompt();
    return;
  }

  m_data.guiData().m_showConfirmCloseAppPopup = true;
  GlfwWrapper::postEmptyEvent();
}

void EntropyApp::quitAppWithoutPrompt()
{
  m_data.state().setQuitApp(true);
  GlfwWrapper::postEmptyEvent();
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
        GlfwWrapper::postEmptyEvent();
        return;
      }
      quitAppWithoutPrompt();
      return;
  }
}

void EntropyApp::closeProject()
{
  const std::size_t imageCount = m_data.numImages();
  if (const auto& projectFileName = m_data.projectFileName()) {
    spdlog::info("Closing project {} containing {} image(s)", *projectFileName, imageCount);
  }
  else if (imageCount > 0) {
    spdlog::info("Closing unsaved project containing {} image(s)", imageCount);
  }

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

  m_data.renderResources().clearLoadedData();
  m_data.renderDerivedData().clear();

  m_data.clearProjectData();
  m_data.guiData().m_renderUiWindows = true;
  m_data.guiData().m_renderUiOverlays = true;
  m_data.guiData().m_viewOverlayControlExtents.clear();
  m_rendering.setVectorOverlayVisibility(Rendering::VectorOverlayVisibility::Configured);
  m_glfw.setWindowTitleStatus("");
  m_glfw.setEventProcessingMode(EventProcessingMode::Wait);
  GlfwWrapper::postEmptyEvent();
}
