#include "ui/ImGuiWrapper.h"

#include "logic/app/CallbackHandler.h"
#include "logic/app/Data.h"
#include "logic/app/StackTrace.h"
#include "logic/camera/CameraHelpers.h"
#include "logic/states/annotation/AnnotationStateHelpers.h"
#include "logic/states/annotation/AnnotationStateMachine.h"
#include "ui/GradientBackgroundRenderer.h"
#include "ui/Helpers.h"
#include "ui/imgui/Layout.h"
#include "ui/imgui/TimePlayback.h"
#include "ui/imgui/Workspace.h"
#include "ui/popups/Popups.h"
#include "ui/toolbars/Toolbars.h"
#include "ui/windows/ExportStatusWindow.h"
#include "ui/windows/OpacityMixerWindow.h"
#include "ui/windows/Windows.h"

#include <imgui/backends/imgui_impl_opengl3.h>
#include <implot/implot.h>
#include <spdlog/fmt/std.h>
#include <spdlog/spdlog.h>

#include <functional>
#include <utility>

using namespace ui::imgui_detail;

void ImGuiWrapper::annotationToolbar(const std::function<void()>& paintActiveAnnotation)
{
  if (!state::annot::isInStateWhereToolbarVisible()) {
    return;
  }

  if (!ASM::current_state_ptr || !state::annot::AnnotationStateMachine::selectedViewUid()) {
    return;
  }

  // Position the annotation toolbar at the bottom of this view:
  const View* annotationView = m_appData.windowData().getView(*state::annot::AnnotationStateMachine::selectedViewUid());

  const float wholeWindowHeight = static_cast<float>(m_appData.windowData().getWindowSize().y);

  const auto mindowAnnotViewFrameBounds = helper::computeMindowFrameBounds(
    annotationView->windowClipViewport(),
    m_appData.windowData().viewport().getAsVec4(),
    wholeWindowHeight);

  renderAnnotationToolbar(m_appData, mindowAnnotViewFrameBounds, paintActiveAnnotation);
}

bool ImGuiWrapper::renderApplicationWindows(
  bool loadingOrImporting,
  bool hasLoadedProject,
  ProjectLoadState projectLoadState,
  bool backgroundTaskRunning)
{
  using namespace std::placeholders;
  const auto getActiveImageIndex = std::bind_front(&ImGuiWrapper::getActiveImageIndex, this);
  const auto setActiveImageIndex = std::bind_front(&ImGuiWrapper::setActiveImageIndex, this);
  const auto getImageHasActiveSeg = std::bind_front(&ImGuiWrapper::getImageHasActiveSeg, this);
  const auto setImageHasActiveSeg = std::bind_front(&ImGuiWrapper::setImageHasActiveSeg, this);
  const auto getMouseMode = std::bind_front(&ImGuiWrapper::getMouseMode, this);
  const auto setMouseMode = std::bind_front(&ImGuiWrapper::setMouseMode, this);
  const auto cycleViewLayout = std::bind_front(&ImGuiWrapper::cycleViewLayout, this);
  const auto getNumImageColorMaps = std::bind_front(&ImGuiWrapper::getNumImageColorMaps, this);
  const auto getImageColorMap = std::bind_front(&ImGuiWrapper::getImageColorMap, this);
  const auto getLabelTable = std::bind_front(&ImGuiWrapper::getLabelTable, this);
  const auto moveImageBackward = std::bind_front(&ImGuiWrapper::moveImageBackward, this);
  const auto moveImageForward = std::bind_front(&ImGuiWrapper::moveImageForward, this);
  const auto moveImageToBack = std::bind_front(&ImGuiWrapper::moveImageToBack, this);
  const auto moveImageToFront = std::bind_front(&ImGuiWrapper::moveImageToFront, this);
  const auto setViewCameraDirection = std::bind_front(&ImGuiWrapper::setViewCameraDirection, this);
  if (m_appData.guiData().m_renderUiWindows) {
    renderConfirmSetReferenceImagePopup(m_appData, m_setReferenceImage);
    renderConfirmRemoveImagePopup(m_appData, m_removeImage);
    renderLargeImageLoadPromptPopup(m_appData, m_largeImageLoadDecision);
    renderRasterImageHeaderPromptPopup(m_appData, m_rasterImageHeaderDecision);
    renderDicomFolderPathPopup(m_appData, m_openDicomFolders);
    renderDicomSeriesSelectionPopup(m_appData, m_loadDicomSeries);

    if (m_appData.guiData().m_showImGuiDemoWindow) {
      ImGui::ShowDemoWindow(&m_appData.guiData().m_showImGuiDemoWindow);
    }

    if (m_appData.guiData().m_showImPlotDemoWindow) {
      ImPlot::ShowDemoWindow(&m_appData.guiData().m_showImPlotDemoWindow);
    }

    renderMenus(projectLoadState, backgroundTaskRunning);

    if (ProjectLoadState::Loading == projectLoadState) {
      ui::renderGradientBackground();
    }

    if (backgroundTaskRunning) {
      renderLoadingStatusWindow(m_appData.guiData());
    }
    renderMeshExtractionStatusWindow(m_appData.guiData());
    if (m_appData.guiData().m_exportJobs) {
      ui::export_jobs::renderStatusWindow(*m_appData.guiData().m_exportJobs);
    }
    renderWarpInversionProgressPopup();
    ui::updates::renderUpdateCheckWindow(
      m_updateCheckWindowState,
      m_appData.settings().automaticUpdateChecksEnabled(),
      [this](bool enabled) { m_appData.settings().setAutomaticUpdateChecksEnabled(enabled); });

    if (hasLoadedProject) {
      renderGlobalTimeControl(m_appData);
    }

    renderSettingsPanel();

    if (!hasLoadedProject) {
      ui::renderKeyboardShortcutsWindow(m_appData.guiData().m_showKeyboardShortcutsWindow);
      renderAboutDialogModalPopup(m_appData.guiData().m_showAboutDialog);
      m_appData.guiData().m_showAboutDialog = false;

      if (
        m_postEmptyGlfwEvent &&
        (ImGui::IsAnyItemActive() || ImGui::IsMouseClicked(ImGuiMouseButton_Left) ||
         ImGui::IsMouseReleased(ImGuiMouseButton_Left) || ImGui::IsMouseClicked(ImGuiMouseButton_Right) ||
         ImGui::IsMouseReleased(ImGuiMouseButton_Right)))
      {
        m_postEmptyGlfwEvent();
      }
      ImGui::Render();
      ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
      return false;
    }

    ui::renderKeyboardShortcutsWindow(m_appData.guiData().m_showKeyboardShortcutsWindow);

    if (!loadingOrImporting) {
      requestMissingComponentProjectionImages();
    }

    if (m_appData.guiData().m_showIsosurfacesWindow) {
      renderIsosurfacesWindow(
        m_appData,
        std::bind(&ImGuiWrapper::storeFuture, this, _1, _2),
        std::bind(&ImGuiWrapper::addTaskToIsosurfaceGpuMeshGenerationQueue, this, _1),
        m_exportIsosurfaceMesh);
    }

    if (m_appData.guiData().m_showInspectionWindow) {
      try {
        renderInspectionWindowWithTable(
          m_appData,
          std::bind(&ImGuiWrapper::getImageDisplayAndFileNames, this, _1),
          m_getSubjectPos,
          m_getVoxelPos,
          m_setSubjectPos,
          m_setVoxelPos,
          m_getImageValuesNN,
          m_getImageValuesLinear,
          m_getSegLabel,
          getLabelTable,
          m_updateImageUniforms);
      }
      catch (const std::exception& e) {
        spdlog::error("Exception while rendering voxel inspector: {}\n{}", e.what(), stack_trace::current(1));
        throw;
      }
    }

    if (m_appData.guiData().m_showImagePropertiesWindow) {
      renderImagePropertiesWindow(
        m_appData,
        m_appData.numImages(),
        std::bind(&ImGuiWrapper::getImageDisplayAndFileNames, this, _1),
        getActiveImageIndex,
        setActiveImageIndex,
        getNumImageColorMaps,
        getImageColorMap,
        moveImageBackward,
        moveImageForward,
        moveImageToBack,
        moveImageToFront,
        m_updateAllImageUniforms,
        m_updateImageUniforms,
        m_updateImageInterpolationMode,
        m_updateImageColorMapInterpolationMode,
        m_loadAndAssignDeformationField,
        [this](
          const uuids::uuid& imageUid,
          const uuids::uuid& sourceWarpUid,
          ComputedWarpDirection direction,
          const WarpInversionOptions& options) { requestWarpInversion(imageUid, sourceWarpUid, direction, options); },
        m_setLockManualImageTransformation,
        [this](const uuids::uuid& imageUid, ComponentProjectionMode mode) {
          requestComponentProjectionImage(imageUid, mode);
        },
        m_importSurfaceMeshes,
        [this](const uuids::uuid& imageUid) {
          m_appData.guiData().m_pendingReferenceImageUid = imageUid;
          m_appData.guiData().m_showConfirmSetReferenceImagePopup = true;
        },
        [this](const uuids::uuid& imageUid) {
          m_appData.guiData().m_pendingRemoveImageUid = imageUid;
          m_appData.guiData().m_showConfirmRemoveImagePopup = true;
        },
        m_recenterAllViews);
    }

    if (m_appData.guiData().m_showSegmentationsWindow) {
      renderSegmentationPropertiesWindow(
        m_appData,
        getLabelTable,
        m_updateImageUniforms,
        m_updateLabelColorTableTexture,
        m_moveCrosshairsToSegLabelCentroid,
        m_createBlankSeg,
        m_addSegmentationFileToImage,
        m_clearSeg,
        m_removeSeg,
        m_exportSegmentationLabelMesh,
        m_exportAllSegmentationLabelMeshes,
        m_recenterAllViews);
    }

    if (m_appData.guiData().m_showRegionStatisticsWindow) {
      renderRegionStatisticsWindow(m_appData, m_regionStatisticsController);
    }

    if (m_appData.guiData().m_showLandmarksWindow) {
      renderLandmarkPropertiesWindow(m_appData, m_recenterAllViews);
    }

    if (m_appData.guiData().m_showAnnotationsWindow) {
      renderAnnotationWindow(
        m_appData,
        setViewCameraDirection,
        m_paintActiveSegmentationWithActivePolygon,
        m_recenterAllViews);
    }

    if (m_appData.guiData().m_showOpacityBlenderWindow) {
      renderOpacityBlenderWindow(m_appData, m_updateImageUniforms);
    }

    if (m_appData.guiData().m_showRegistrationSetupWindow) {
      renderRegistrationSetupWindow(m_appData);
    }

    if (m_appData.guiData().m_showRegistrationJobsWindow) {
      renderRegistrationJobsWindow(m_appData, [this](const std::string& jobId) {
        if (m_importRegistrationJobOutputs) {
          m_importRegistrationJobOutputs(jobId);
        }
      });
    }

    renderRegistrationProgressWindow(m_appData);

    renderModeToolbar(
      m_appData,
      getMouseMode,
      setMouseMode,
      m_readjustViewport,
      m_recenterAllViews,
      m_getOverlayVisibility,
      m_setOverlayVisibility,
      cycleViewLayout,

      m_appData.numImages(),
      std::bind(&ImGuiWrapper::getImageDisplayAndFileNames, this, _1),
      getActiveImageIndex,
      setActiveImageIndex);

    renderLayoutTabs(m_appData);

    renderAddLayoutModalPopup(m_appData, m_appData.guiData().m_showAddLayoutPopup, [this]() {
      if (m_recenterAllViews) {
        m_recenterAllViews(false, false, true, false, true);
      }
    });
    m_appData.guiData().m_showAddLayoutPopup = false;

    renderConfirmRemoveLayoutPopup(m_appData);

    renderAboutDialogModalPopup(m_appData.guiData().m_showAboutDialog);
    m_appData.guiData().m_showAboutDialog = false;

    renderSegToolbar(
      m_appData,
      m_appData.numImages(),
      std::bind(&ImGuiWrapper::getImageDisplayAndFileNames, this, _1),
      getActiveImageIndex,
      setActiveImageIndex,
      getImageHasActiveSeg,
      setImageHasActiveSeg,
      m_createBlankSeg,
      m_updateImageUniforms,
      setMouseMode,
      m_readjustViewport,
      m_postEmptyGlfwEvent,
      m_executePoissonSeg);

    annotationToolbar(m_paintActiveSegmentationWithActivePolygon);
  }

  return true;
}
