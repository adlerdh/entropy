#include "EntropyApp.h"

#include "logic/app/DataHelper.h"
#include "rendering/TextureSetup.h"

#include <utility>

namespace fs = std::filesystem;

void EntropyApp::setCallbacks()
{
  m_glfw.setCallbacks(
    [this](std::chrono::time_point<std::chrono::steady_clock>& lastFrameTime) {
      m_rendering.framerateLimiter(lastFrameTime);
    },
    [this]() { m_rendering.render(); },
    [this]() { m_imgui.render(); },
    [this]() {
      pollDicomSeriesScan();
      m_itkSnapSync.update();
      m_entropyInstanceSync.update();
      const bool syncEnabled = m_data.settings().cursorSyncEnabled() || m_data.settings().entropyInstanceSyncEnabled();
      if (syncEnabled && !m_data.state().animating()) {
        m_glfw.setEventProcessingMode(EventProcessingMode::WaitTimeout);
        m_glfw.setWaitTimeout(1.0 / 30.0);
      }
      else if (!m_data.state().animating()) {
        m_glfw.setEventProcessingMode(EventProcessingMode::Wait);
      }
    });

  ImGuiWrapperCallbacks imguiCallbacks;

  imguiCallbacks.platform.postEmptyGlfwEvent = [this]() {
    m_glfw.postEmptyEvent();
  };
  imguiCallbacks.platform.readjustViewport = [this]() {
    resize(m_data.windowData().getWindowSize().x, m_data.windowData().getWindowSize().y);
  };

  imguiCallbacks.project.openImageFiles = [this](const std::vector<fs::path>& fileNames) {
    loadImageFiles(fileNames);
  };
  imguiCallbacks.project.addImageFiles = [this](const std::vector<fs::path>& fileNames) {
    addImageFiles(fileNames);
  };
  imguiCallbacks.project.openDicomFolders = [this](const std::vector<fs::path>& folderNames) {
    openDicomSeriesFolders(folderNames);
  };
  imguiCallbacks.project.addSegmentationFile = [this](const fs::path& fileName) {
    addSegmentationFile(fileName);
  };
  imguiCallbacks.project.addSegmentationFileToImage = [this](const uuids::uuid& imageUid, const fs::path& fileName) {
    addSegmentationFileToImage(fileName, imageUid);
  };
  imguiCallbacks.project.loadDeformationField = [this](const fs::path& fileName) -> std::optional<uuids::uuid> {
    const auto [defUid, loaded] = loadDeformationField(fileName);
    if (defUid && loaded) {
      createImageTextures(m_data, std::vector<uuids::uuid>{*defUid});
    }
    return defUid;
  };
  imguiCallbacks.project.openProjectFile = [this](const fs::path& fileName) {
    loadProjectFile(fileName);
  };
  imguiCallbacks.project.largeImageLoadDecision = [this](GuiData::LargeImageLoadDecision decision) {
    handleLargeImageLoadDecision(decision);
  };
  imguiCallbacks.project.loadDicomSeries = [this](
                                             const std::vector<dicom::SeriesInfo>& series,
                                             std::optional<std::size_t> referenceSeriesIndex,
                                             bool addToExistingProject) {
    loadDicomSeries(series, referenceSeriesIndex, addToExistingProject);
  };
  imguiCallbacks.project.saveProject = [this]() {
    return saveProject();
  };
  imguiCallbacks.project.saveProjectAs = [this](const fs::path& fileName) {
    return saveProjectAs(fileName);
  };
  imguiCallbacks.project.closeProject = [this]() {
    requestCloseProject();
  };
  imguiCallbacks.project.loadLayoutsFile = [this](const fs::path& fileName) {
    loadLayoutsFile(fileName);
  };
  imguiCallbacks.project.saveLayoutsFile = [this](const fs::path& fileName) {
    return saveLayoutsFile(fileName);
  };
  imguiCallbacks.project.closeProjectWithoutPrompt = [this]() {
    continueAfterUnsavedProjectPrompt();
  };
  imguiCallbacks.project.requestQuitApp = [this]() {
    requestQuitApp();
  };
  imguiCallbacks.project.quitAppWithoutPrompt = [this]() {
    quitAppWithoutPrompt();
  };

  imguiCallbacks.view.recenterView = [this](const uuids::uuid& viewUid) {
    m_callbackHandler.recenterView(m_data.state().recenteringMode(), viewUid);
  };

  imguiCallbacks.view.recenterCurrentViews = [this](
                                               bool recenterCrosshairs,
                                               bool realignCrosshairs,
                                               bool recenterOnCurrentCrosshairsPosition,
                                               bool resetObliqueOrientation,
                                               bool resetZoom) {
    m_callbackHandler.recenterViews(
      m_data.state().recenteringMode(),
      recenterCrosshairs,
      realignCrosshairs,
      recenterOnCurrentCrosshairsPosition,
      resetObliqueOrientation,
      resetZoom);
  };

  imguiCallbacks.view.getOverlayVisibility = [this]() {
    return m_callbackHandler.showOverlays();
  };
  imguiCallbacks.view.setOverlayVisibility = [this](bool show) {
    m_callbackHandler.setShowOverlays(show);
  };
  imguiCallbacks.view.updateAllImageUniforms = [this]() {
    m_rendering.updateImageUniforms(m_data.imageUidsOrdered());
  };
  imguiCallbacks.view.updateImageUniforms = [this](const uuids::uuid& imageUid) {
    m_rendering.updateImageUniforms(imageUid);
  };
  imguiCallbacks.view.updateImageInterpolationMode = [this](const uuids::uuid& imageUid) {
    m_rendering.updateImageInterpolation(imageUid);
  };
  imguiCallbacks.view.updateImageColorMapInterpolationMode = [this](std::size_t cmapIndex) {
    m_rendering.updateImageColorMapInterpolation(cmapIndex);
  };
  imguiCallbacks.view.updateLabelColorTableTexture = [this](size_t labelColorTableIndex) {
    m_rendering.updateLabelColorTableTexture(labelColorTableIndex);
  };

  imguiCallbacks.view.moveCrosshairsToSegLabelCentroid = [this](const uuids::uuid& imageUid, size_t labelIndex) {
    m_callbackHandler.moveCrosshairsToSegLabelCentroid(imageUid, labelIndex);
  };

  imguiCallbacks.view.updateMetricUniforms = [this]() {
    m_rendering.updateMetricUniforms();
  };
  imguiCallbacks.inspection.getWorldDeformedPos = [this]() {
    return m_data.state().worldCrosshairs().worldOrigin();
  };

  imguiCallbacks.inspection.getSubjectPos = [this](size_t imageIndex) -> std::optional<glm::vec3> {
    const auto imageUid = m_data.imageUid(imageIndex);
    const Image* image = imageUid ? m_data.image(*imageUid) : nullptr;
    if (!image) {
      return std::nullopt;
    }

    const glm::vec4 subjectPos =
      image->transformations().subject_T_worldDef() * glm::vec4{m_data.state().worldCrosshairs().worldOrigin(), 1.0f};

    return glm::vec3{subjectPos / subjectPos.w};
  };

  imguiCallbacks.inspection.getVoxelPos = [this](size_t imageIndex) {
    return data::getImageVoxelCoordsAtCrosshairs(m_data, imageIndex);
  };

  imguiCallbacks.inspection.setSubjectPos = [this](size_t imageIndex, const glm::vec3& subjectPos) {
    const auto imageUid = m_data.imageUid(imageIndex);
    const Image* image = imageUid ? m_data.image(*imageUid) : nullptr;
    if (!image) {
      return;
    }

    const glm::vec4 worldPos = image->transformations().worldDef_T_subject() * glm::vec4{subjectPos, 1.0f};

    m_data.state().setWorldCrosshairsPos(glm::vec3{worldPos / worldPos.w});
  };

  imguiCallbacks.inspection.setVoxelPos = [this](size_t imageIndex, const glm::ivec3& voxelPos) {
    const auto imageUid = m_data.imageUid(imageIndex);
    const Image* image = imageUid ? m_data.image(*imageUid) : nullptr;
    if (!image) {
      return;
    }

    // TODO: Put this in CallbackHandler as separate function, because it is used frequently.
    // TODO: All logic related to rounding crosshairs positions should be in one place.

    const glm::vec4 worldPos = image->transformations().worldDef_T_pixel() * glm::vec4{voxelPos, 1.0f};
    const glm::vec3 worldPosRounded =
      data::roundPointToNearestImageVoxelCenter(*image, glm::vec3{worldPos / worldPos.w});

    m_data.state().setWorldCrosshairsPos(worldPosRounded);
  };

  imguiCallbacks.inspection.getImageValuesNN =
    [this](size_t imageIndex, bool getOnlyActiveComponent) -> std::vector<double> {
    std::vector<double> values;

    const auto imageUid = m_data.imageUid(imageIndex);
    const Image* image = imageUid ? m_data.image(*imageUid) : nullptr;
    if (!image) {
      return values;
    }

    if (const auto coords = data::getImageVoxelCoordsAtCrosshairs(m_data, imageIndex)) {
      if (getOnlyActiveComponent) {
        if (const auto a = image->value<double>(image->settings().activeComponent(), coords->x, coords->y, coords->z)) {
          // Return empty vector if component has undefined value
          values.push_back(*a);
        }
        else {
          return std::vector<double>{};
        }
      }
      else {
        for (uint32_t i = 0; i < image->header().numComponentsPerPixel(); ++i) {
          if (const auto a = image->value<double>(i, coords->x, coords->y, coords->z)) {
            values.push_back(*a);
          }
          else {
            // Return empty vector if any component has undefined value
            return std::vector<double>{};
          }
        }
      }
    }

    return values;
  };

  imguiCallbacks.inspection.getImageValuesLinear =
    [this](size_t imageIndex, bool getOnlyActiveComponent) -> std::vector<double> {
    std::vector<double> values;

    const auto imageUid = m_data.imageUid(imageIndex);
    const Image* image = imageUid ? m_data.image(*imageUid) : nullptr;
    if (!image) {
      return values;
    }

    if (const auto coords = data::getImageVoxelCoordsContinuousAtCrosshairs(m_data, imageIndex)) {
      if (getOnlyActiveComponent) {
        if (
          const auto a =
            image->valueLinear<double>(image->settings().activeComponent(), coords->x, coords->y, coords->z))
        {
          // Return empty vector if component has undefined value
          values.push_back(*a);
        }
        else {
          return std::vector<double>{};
        }
      }
      else {
        for (uint32_t i = 0; i < image->header().numComponentsPerPixel(); ++i) {
          if (const auto a = image->valueLinear<double>(i, coords->x, coords->y, coords->z)) {
            values.push_back(*a);
          }
          else {
            // Return empty vector if any component has undefined value
            return std::vector<double>{};
          }
        }
      }
    }

    return values;
  };

  imguiCallbacks.inspection.getSegLabel = [this](size_t imageIndex) -> std::optional<int64_t> {
    const auto imageUid = m_data.imageUid(imageIndex);
    if (!imageUid) {
      return std::nullopt;
    }

    const auto segUid = m_data.imageToActiveSegUid(*imageUid);
    const Image* seg = segUid ? m_data.seg(*segUid) : nullptr;
    if (!seg) {
      return std::nullopt;
    }

    if (const auto coords = data::getSegVoxelCoordsAtCrosshairs(m_data, *segUid, *imageUid)) {
      const uint32_t activeComp = seg->settings().activeComponent();
      return seg->value<int64_t>(activeComp, coords->x, coords->y, coords->z);
    }

    return std::nullopt;
  };

  imguiCallbacks.editing.createBlankSeg =
    [this](const uuids::uuid& matchingImageUid, const std::string& segDisplayName) {
      return m_callbackHandler.createBlankSegWithColorTableAndTextures(matchingImageUid, segDisplayName);
    };

  imguiCallbacks.editing.clearSeg = [this](const uuids::uuid& segUid) -> bool {
    return m_callbackHandler.clearSegVoxels(segUid);
  };

  imguiCallbacks.editing.removeSeg = [this](const uuids::uuid& segUid) -> bool {
    bool success = false;
    success |= m_data.removeSeg(segUid);
    success |= m_rendering.removeSegTexture(segUid);
    return success;
  };

  imguiCallbacks.editing.executePoissonSeg =
    [this](const uuids::uuid& imageUid, const uuids::uuid& seedSegUid, const SeedSegmentationType& segType) -> bool {
    return m_callbackHandler.executePoissonSegmentation(imageUid, seedSegUid, segType);
  };

  imguiCallbacks.editing.setLockManualImageTransformation = [this](const uuids::uuid& imageUid, bool locked) -> bool {
    return m_callbackHandler.setLockManualImageTransformation(imageUid, locked);
  };

  imguiCallbacks.editing.setReferenceImage = [this](const uuids::uuid& imageUid) -> bool {
    return setReferenceImage(imageUid);
  };

  imguiCallbacks.editing.removeImage = [this](const uuids::uuid& imageUid) -> bool {
    return removeImage(imageUid);
  };

  imguiCallbacks.editing.paintActiveSegmentationWithActivePolygon = [this]() {
    return m_callbackHandler.paintActiveSegmentationWithAnnotation();
  };

  m_imgui.setCallbacks(std::move(imguiCallbacks));
}
