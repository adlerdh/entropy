#define IMGUI_DEFINE_MATH_OPERATORS
#include "ui/ImGuiWrapper.h"

#include "logic/app/CallbackHandler.h"
#include "logic/app/Data.h"
#include "logic/states/annotation/AnnotationStateHelpers.h"
#include "logic/states/annotation/AnnotationStateMachine.h"
#include "registration/AffineTransformIO.h"
#include "ui/ImageExport.h"
#include "ui/NativeFileDialogs.h"
#include "ui/dialogs/InputLoadErrorDialog.h"
#include "ui/dialogs/NativeMessageDialogs.h"
#include "ui/imgui/ImageSelection.h"
#include "ui/imgui/Layout.h"
#include "ui/imgui/TimePlayback.h"
#include "ui/menus/MainMenuBar.h"

#include <imgui/imgui_internal.h>
#include <spdlog/fmt/std.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <functional>
#include <utility>

using namespace ui::imgui_detail;

void ImGuiWrapper::performMenuAction(MainMenuAction action)
{
  const auto mirrorInitialAffineToSegmentations = [this](const uuids::uuid& imageUid, const Image& image) {
    for (const uuids::uuid& segUid : m_appData.imageToSegUids(imageUid)) {
      if (Image* seg = m_appData.seg(segUid)) {
        auto& segTx = seg->transformations();
        segTx.set_enable_affine_T_subject(image.transformations().get_enable_affine_T_subject());
        segTx.set_affine_T_subject(image.transformations().get_affine_T_subject());
        segTx.set_affine_T_subject_fileName(image.transformations().get_affine_T_subject_fileName());
      }
    }
  };

  const auto updateAffineUniforms = [this]() {
    if (m_updateAllImageUniforms) {
      m_updateAllImageUniforms();
    }
  };

  const auto confirmResetAffineTransformation =
    [](const char* title, const char* message, const char* informativeText, const char* resetButton) {
      const auto result =
        native_dialog::showMessageDialog({title, message, informativeText, resetButton, "Cancel", ""});
      return result && native_dialog::MessageDialogResult::FirstButton == *result;
    };

  switch (action) {
    case MainMenuAction::SetModePointer:
      m_callbackHandler.setMouseMode(MouseMode::Pointer);
      break;
    case MainMenuAction::SetModeWindowLevel:
      m_callbackHandler.setMouseMode(MouseMode::WindowLevel);
      break;
    case MainMenuAction::SetModeZoom:
      m_callbackHandler.setMouseMode(MouseMode::CameraZoom);
      break;
    case MainMenuAction::SetModePan:
      m_callbackHandler.setMouseMode(MouseMode::CameraTranslate);
      break;
    case MainMenuAction::SetModeRotateView:
      m_callbackHandler.setMouseMode(MouseMode::CameraRotate);
      break;
    case MainMenuAction::SetModeRotateCrosshairs:
      m_callbackHandler.setMouseMode(MouseMode::CrosshairsRotate);
      break;
    case MainMenuAction::SetModeSegment:
      m_callbackHandler.setMouseMode(MouseMode::Segment);
      break;
    case MainMenuAction::SetModeAnnotate:
      m_callbackHandler.setMouseMode(MouseMode::Annotate);
      break;
    case MainMenuAction::SetModeTranslateImage:
      m_callbackHandler.setMouseMode(MouseMode::ImageTranslate);
      break;
    case MainMenuAction::SetModeRotateImage:
      m_callbackHandler.setMouseMode(MouseMode::ImageRotate);
      break;
    case MainMenuAction::SetModeScaleImage:
      m_callbackHandler.setMouseMode(MouseMode::ImageScale);
      break;
    case MainMenuAction::Recenter:
      if (m_recenterAllViews) m_recenterAllViews(false, false, true, false, true);
      break;
    case MainMenuAction::ResetView:
      if (m_recenterAllViews) m_recenterAllViews(true, true, true, true, true);
      break;
    case MainMenuAction::ToggleSynchronizeThreeDCameras:
      m_appData.renderSettings().m_synchronizeThreeDCameras = !m_appData.renderSettings().m_synchronizeThreeDCameras;
      if (m_appData.renderSettings().m_synchronizeThreeDCameras) {
        m_appData.windowData().synchronizeCurrentLayoutThreeDCameras();
      }
      break;
    case MainMenuAction::ToggleImageVisibility:
      m_callbackHandler.toggleImageVisibility();
      break;
    case MainMenuAction::ToggleSegmentationVisibility:
      m_callbackHandler.toggleSegVisibility();
      break;
    case MainMenuAction::ToggleImageEdges:
      m_callbackHandler.toggleImageEdges();
      break;
    case MainMenuAction::ToggleSegmentationOutline:
      m_callbackHandler.toggleSegGlobalOutline();
      break;
    case MainMenuAction::DecreaseActiveImageOpacity:
      m_callbackHandler.changeImageOpacity(-0.05);
      break;
    case MainMenuAction::IncreaseActiveImageOpacity:
      m_callbackHandler.changeImageOpacity(0.05);
      break;
    case MainMenuAction::DecreaseSegmentationOpacity:
      m_callbackHandler.changeSegOpacity(-0.05, false);
      break;
    case MainMenuAction::IncreaseSegmentationOpacity:
      m_callbackHandler.changeSegOpacity(0.05, false);
      break;
    case MainMenuAction::ToggleCrosshairsVoxelSnapping:
      m_appData.renderSettings().m_snapCrosshairs =
        CrosshairsSnapping::Disabled == m_appData.renderSettings().m_snapCrosshairs ? CrosshairsSnapping::ReferenceImage
                                                                                    : CrosshairsSnapping::Disabled;
      break;
    case MainMenuAction::ToggleCrosshairs:
      m_callbackHandler.toggleCrosshairs();
      break;
    case MainMenuAction::ToggleScaleBars:
      m_appData.renderSettings().m_showScaleBars = !m_appData.renderSettings().m_showScaleBars;
      m_appData.renderSettings().m_showScaleBarsInLightboxViews = m_appData.renderSettings().m_showScaleBars;
      break;
    case MainMenuAction::ToggleAsciiRendering:
      m_appData.renderSettings().m_asciiEnabled = !m_appData.renderSettings().m_asciiEnabled;
      break;
    case MainMenuAction::ToggleLightboxOffsets:
      m_appData.renderSettings().m_showLightboxOffsetLabels = !m_appData.renderSettings().m_showLightboxOffsetLabels;
      break;
    case MainMenuAction::CycleViewOverlays:
      m_callbackHandler.cycleViewOverlays();
      break;
    case MainMenuAction::ToggleUserInterface:
      m_callbackHandler.setShowUserInterface(!m_callbackHandler.showUserInterface());
      break;
    case MainMenuAction::ToggleFullScreen:
      m_callbackHandler.toggleFullScreenMode();
      break;
    case MainMenuAction::ToggleEntropyInstanceSync:
      m_appData.settings().setEntropyInstanceSyncEnabled(!m_appData.settings().entropyInstanceSyncEnabled());
      break;
    case MainMenuAction::ToggleSync:
      m_appData.settings().setCursorSyncEnabled(!m_appData.settings().cursorSyncEnabled());
      break;
    case MainMenuAction::ToggleSyncSendCursor:
      m_appData.settings().setSendCursorSync(!m_appData.settings().sendCursorSync());
      break;
    case MainMenuAction::ToggleSyncReceiveCursor:
      m_appData.settings().setReceiveCursorSync(!m_appData.settings().receiveCursorSync());
      break;
    case MainMenuAction::ToggleSyncSendZoom:
      m_appData.settings().setSendZoomSync(!m_appData.settings().sendZoomSync());
      break;
    case MainMenuAction::ToggleSyncReceiveZoom:
      m_appData.settings().setReceiveZoomSync(!m_appData.settings().receiveZoomSync());
      break;
    case MainMenuAction::ToggleSyncSendPan:
      m_appData.settings().setSendPanSync(!m_appData.settings().sendPanSync());
      break;
    case MainMenuAction::ToggleSyncReceivePan:
      m_appData.settings().setReceivePanSync(!m_appData.settings().receivePanSync());
      break;
    case MainMenuAction::SetActiveImageAsReference:
      if (const auto imageUid = activeImageUid(); imageUid && imageUid != m_appData.refImageUid()) {
        m_appData.guiData().m_pendingReferenceImageUid = *imageUid;
        m_appData.guiData().m_showConfirmSetReferenceImagePopup = true;
      }
      break;
    case MainMenuAction::ActivatePreviousImage:
      m_callbackHandler.cycleActiveImage(-1);
      break;
    case MainMenuAction::ActivateNextImage:
      m_callbackHandler.cycleActiveImage(1);
      break;
    case MainMenuAction::ExportActiveImage:
      if (const auto imageUid = activeImageUid()) {
        image_export::exportImage(m_appData, *imageUid);
      }
      break;
    case MainMenuAction::RemoveActiveImage:
      if (const auto imageUid = activeImageUid()) {
        m_appData.guiData().m_pendingRemoveImageUid = *imageUid;
        m_appData.guiData().m_showConfirmRemoveImagePopup = true;
      }
      break;
    case MainMenuAction::MoveActiveImageBackward:
      if (const auto imageUid = activeImageUid()) moveImageBackward(*imageUid);
      break;
    case MainMenuAction::MoveActiveImageForward:
      if (const auto imageUid = activeImageUid()) moveImageForward(*imageUid);
      break;
    case MainMenuAction::MoveActiveImageToBack:
      if (const auto imageUid = activeImageUid()) moveImageToBack(*imageUid);
      break;
    case MainMenuAction::MoveActiveImageToFront:
      if (const auto imageUid = activeImageUid()) moveImageToFront(*imageUid);
      break;
    case MainMenuAction::ToggleActiveImageTransformationLock:
      if (const auto imageUid = activeImageUid()) {
        const Image* image = m_appData.image(*imageUid);
        if (image && m_setLockManualImageTransformation) {
          m_setLockManualImageTransformation(*imageUid, !image->transformations().is_worldDef_T_affine_locked());
        }
      }
      break;
    case MainMenuAction::LoadActiveImageInitialTransformation:
      if (const auto imageUid = activeImageUid()) {
        Image* image = m_appData.image(*imageUid);
        const auto selectedFile = image ? native_dialog::openFile(native_dialog::transformFilters()) : std::nullopt;
        if (image && selectedFile) {
          glm::dmat4 affine_T_subject{1.0};
          if (serialize::openAffineTxFile(affine_T_subject, *selectedFile)) {
            image->transformations().set_enable_affine_T_subject(true);
            image->transformations().set_affine_T_subject(glm::mat4{affine_T_subject});
            image->transformations().set_affine_T_subject_fileName(selectedFile);
            mirrorInitialAffineToSegmentations(*imageUid, *image);
            updateAffineUniforms();
            spdlog::info("Loaded initial affine transformation matrix from file {}", *selectedFile);
          }
          else {
            spdlog::error("Error loading initial affine transformation matrix from file {}", *selectedFile);
            native_dialog::showInputLoadErrorDialog(
              {.inputType = "affine transformation",
               .path = selectedFile,
               .cause = "The transformation matrix file could not be read or parsed."});
          }
        }
      }
      break;
    case MainMenuAction::SaveActiveImageInitialTransformation:
      if (const auto imageUid = activeImageUid()) {
        const Image* image = m_appData.image(*imageUid);
        const auto selectedFile = image ? native_dialog::saveFile(native_dialog::transformFilters()) : std::nullopt;
        if (image && selectedFile) {
          const glm::dmat4 affine_T_subject{image->transformations().get_affine_T_subject()};
          if (serialize::saveAffineTxFile(affine_T_subject, *selectedFile)) {
            spdlog::info("Saved initial affine transformation matrix to file {}", *selectedFile);
          }
          else {
            spdlog::error("Error saving initial affine transformation matrix to file {}", *selectedFile);
          }
        }
      }
      break;
    case MainMenuAction::ResetActiveImageInitialTransformation:
      if (const auto imageUid = activeImageUid()) {
        if (Image* image = m_appData.image(*imageUid)) {
          if (confirmResetAffineTransformation(
                "Reset initial affine?",
                "Reset the initial/imported affine transformation to identity?",
                "This clears the source transform file and cannot be undone.",
                "Reset Initial Affine"))
          {
            image->transformations().set_enable_affine_T_subject(true);
            image->transformations().set_affine_T_subject(glm::mat4{1.0f});
            image->transformations().set_affine_T_subject_fileName(std::nullopt);
            mirrorInitialAffineToSegmentations(*imageUid, *image);
            updateAffineUniforms();
          }
        }
      }
      break;
    case MainMenuAction::ResetActiveImageManualTransformation:
      if (const auto imageUid = activeImageUid()) {
        Image* image = m_appData.image(*imageUid);
        if (image) {
          if (confirmResetAffineTransformation(
                "Reset manual affine?",
                "Reset the manual affine transformation to identity?",
                "This clears manual translation, rotation, and scale adjustments and cannot be undone.",
                "Reset Manual Affine"))
          {
            image->transformations().reset_worldDef_T_affine();
            if (m_updateImageUniforms) {
              m_updateImageUniforms(*imageUid);
            }
          }
        }
      }
      break;
    case MainMenuAction::SaveActiveImageManualTransformation:
      if (const auto imageUid = activeImageUid()) {
        const Image* image = m_appData.image(*imageUid);
        const auto selectedFile = image ? native_dialog::saveFile(native_dialog::transformFilters()) : std::nullopt;
        if (selectedFile) {
          const glm::dmat4 worldDef_T_affine{image->transformations().get_worldDef_T_affine()};
          if (serialize::saveAffineTxFile(worldDef_T_affine, *selectedFile)) {
            spdlog::info("Saved manual affine transformation matrix to file {}", *selectedFile);
          }
          else {
            spdlog::error("Error saving manual affine transformation matrix to file {}", *selectedFile);
          }
        }
      }
      break;
    case MainMenuAction::SaveActiveImageEffectiveTransformation:
      if (const auto imageUid = activeImageUid()) {
        const Image* image = m_appData.image(*imageUid);
        const auto selectedFile = image ? native_dialog::saveFile(native_dialog::transformFilters()) : std::nullopt;
        if (selectedFile) {
          const auto& tx = image->transformations();
          const glm::dmat4 affine_T_subject{tx.get_affine_T_subject()};
          const glm::dmat4 worldDef_T_affine{tx.get_worldDef_T_affine()};
          if (serialize::saveAffineTxFile(worldDef_T_affine * affine_T_subject, *selectedFile)) {
            spdlog::info("Saved effective affine transformation matrix to file {}", *selectedFile);
          }
          else {
            spdlog::error("Error saving effective affine transformation matrix to file {}", *selectedFile);
          }
        }
      }
      break;
    case MainMenuAction::ToggleApplyActiveImageWarp:
      if (const auto imageUid = activeImageUid()) {
        if (Image* image = m_appData.image(*imageUid)) {
          image->settings().setWarpEnabled(!image->settings().warpEnabled());
          if (m_updateImageUniforms) {
            m_updateImageUniforms(*imageUid);
          }
        }
      }
      break;
    case MainMenuAction::ShowRegistrationSetupWindow:
      m_appData.guiData().m_showRegistrationSetupWindow = true;
      break;
    case MainMenuAction::ToggleRegistrationJobsWindow:
      m_appData.guiData().m_showRegistrationJobsWindow = !m_appData.guiData().m_showRegistrationJobsWindow;
      break;
    case MainMenuAction::ShowOpacityMixer:
      m_appData.guiData().m_showOpacityBlenderWindow = !m_appData.guiData().m_showOpacityBlenderWindow;
      break;
    case MainMenuAction::ToggleGlobalTimeControls:
      m_appData.settings().setShowGlobalTimeControls(!m_appData.settings().showGlobalTimeControls());
      break;
    case MainMenuAction::ToggleTimePlayback:
      m_callbackHandler.toggleTimePlayback();
      break;
    case MainMenuAction::FirstTimePoint:
      m_callbackHandler.setTimePointToFirst();
      break;
    case MainMenuAction::PreviousTimePoint:
      m_callbackHandler.cycleTimePoint(-1);
      break;
    case MainMenuAction::NextTimePoint:
      m_callbackHandler.cycleTimePoint(1);
      break;
    case MainMenuAction::LastTimePoint:
      m_callbackHandler.setTimePointToLast();
      break;
    case MainMenuAction::AddIsosurface:
      m_appData.guiData().m_showIsosurfacesWindow = true;
      m_appData.guiData().m_requestAddIsosurface = true;
      break;
    case MainMenuAction::AddIsosurfaceRange:
      m_appData.guiData().m_showIsosurfacesWindow = true;
      m_appData.guiData().m_requestAddIsosurfaceRange = true;
      break;
    case MainMenuAction::ImportSurfaceMesh:
      if (const auto imageUid = activeImageUid(); imageUid && m_importSurfaceMeshes) {
        m_importSurfaceMeshes(*imageUid);
      }
      break;
    case MainMenuAction::CreateSegmentation:
      createActiveSegmentation();
      break;
    case MainMenuAction::ExportActiveSegmentation:
      exportActiveSegmentation();
      break;
    case MainMenuAction::ClearSegmentation:
      if (const auto segUid = activeSegUid(); segUid && m_clearSeg) m_clearSeg(*segUid);
      break;
    case MainMenuAction::RemoveSegmentation:
      if (const auto segUid = activeSegUid(); segUid && m_removeSeg && m_removeSeg(*segUid)) {
        if (const auto imageUid = activeImageUid(); imageUid && m_updateImageUniforms) m_updateImageUniforms(*imageUid);
      }
      break;
    case MainMenuAction::PreviousForegroundLabel:
      m_callbackHandler.cycleForegroundSegLabel(-1);
      break;
    case MainMenuAction::NextForegroundLabel:
      m_callbackHandler.cycleForegroundSegLabel(1);
      break;
    case MainMenuAction::PreviousBackgroundLabel:
      m_callbackHandler.cycleBackgroundSegLabel(-1);
      break;
    case MainMenuAction::NextBackgroundLabel:
      m_callbackHandler.cycleBackgroundSegLabel(1);
      break;
    case MainMenuAction::DecreaseBrushSize:
      m_callbackHandler.cycleBrushSize(-1);
      break;
    case MainMenuAction::IncreaseBrushSize:
      m_callbackHandler.cycleBrushSize(1);
      break;
    case MainMenuAction::PaintSegmentationFromAnnotation:
      if (m_paintActiveSegmentationWithActivePolygon) m_paintActiveSegmentationWithActivePolygon();
      break;
    case MainMenuAction::ImportAnnotations:
      importAnnotationsToActiveImage();
      break;
    case MainMenuAction::ExportAnnotations:
      exportAnnotationsForActiveImage();
      break;
    case MainMenuAction::RemoveAnnotation:
      if (const auto [imageUid, annotUid] = activeAnnotation(); imageUid && annotUid) {
        m_appData.removeAnnotation(*annotUid);
        ASM::synchronizeAnnotationHighlights();
      }
      break;
    case MainMenuAction::MoveAnnotationBackward:
      if (const auto [imageUid, annotUid] = activeAnnotation(); imageUid && annotUid) {
        m_appData.moveAnnotationBackwards(*imageUid, *annotUid);
      }
      break;
    case MainMenuAction::MoveAnnotationForward:
      if (const auto [imageUid, annotUid] = activeAnnotation(); imageUid && annotUid) {
        m_appData.moveAnnotationForwards(*imageUid, *annotUid);
      }
      break;
    case MainMenuAction::MoveAnnotationToBack:
      if (const auto [imageUid, annotUid] = activeAnnotation(); imageUid && annotUid) {
        m_appData.moveAnnotationToBack(*imageUid, *annotUid);
      }
      break;
    case MainMenuAction::MoveAnnotationToFront:
      if (const auto [imageUid, annotUid] = activeAnnotation(); imageUid && annotUid) {
        m_appData.moveAnnotationToFront(*imageUid, *annotUid);
      }
      break;
    case MainMenuAction::CreateLandmarkGroup:
      createActiveLandmarkGroup();
      break;
    case MainMenuAction::ImportLandmarkGroup:
      importLandmarkGroupForActiveImage();
      break;
    case MainMenuAction::SaveLandmarkGroup:
      saveActiveLandmarkGroup();
      break;
    case MainMenuAction::RemoveLandmarkGroup:
      removeActiveLandmarkGroup();
      break;
    case MainMenuAction::AddLayout:
      m_appData.guiData().m_showAddLayoutPopup = true;
      break;
    case MainMenuAction::RemoveLayout: {
      const auto& windowData = m_appData.windowData();
      if (windowData.numLayouts() >= 2) {
        requestLayoutRemoval(m_appData, windowData.currentLayoutIndex());
      }
      break;
    }
    case MainMenuAction::ToggleLayoutTabs:
      m_appData.settings().setShowLayoutTabs(!m_appData.settings().showLayoutTabs());
      syncLayoutTabGuiDataFromSettings(m_appData);
      if (m_readjustViewport) {
        m_readjustViewport();
      }
      break;
    case MainMenuAction::ToggleImagesWindow:
      m_appData.guiData().m_showImagePropertiesWindow = !m_appData.guiData().m_showImagePropertiesWindow;
      break;
    case MainMenuAction::ToggleSegmentationsWindow:
      m_appData.guiData().m_showSegmentationsWindow = !m_appData.guiData().m_showSegmentationsWindow;
      break;
    case MainMenuAction::ToggleRegionStatisticsWindow:
      m_appData.guiData().m_showRegionStatisticsWindow = !m_appData.guiData().m_showRegionStatisticsWindow;
      break;
    case MainMenuAction::ToggleLandmarksWindow:
      m_appData.guiData().m_showLandmarksWindow = !m_appData.guiData().m_showLandmarksWindow;
      break;
    case MainMenuAction::ToggleAnnotationsWindow:
      m_appData.guiData().m_showAnnotationsWindow = !m_appData.guiData().m_showAnnotationsWindow;
      break;
    case MainMenuAction::ToggleIsosurfacesWindow:
      m_appData.guiData().m_showIsosurfacesWindow = !m_appData.guiData().m_showIsosurfacesWindow;
      break;
    case MainMenuAction::ToggleSettingsWindow:
      m_appData.guiData().m_showSettingsWindow = !m_appData.guiData().m_showSettingsWindow;
      break;
    case MainMenuAction::ShowSettingsWindow:
      m_appData.guiData().m_showSettingsWindow = true;
      break;
    case MainMenuAction::ShowSynchronizeSettingsWindow:
      m_appData.guiData().m_showSettingsWindow = true;
      m_appData.guiData().m_requestedSettingsTab = GuiData::SettingsTab::Synchronization;
      break;
    case MainMenuAction::ToggleInspectorWindow:
      m_appData.guiData().m_showInspectionWindow = !m_appData.guiData().m_showInspectionWindow;
      break;
    case MainMenuAction::ToggleOpacityMixerWindow:
      m_appData.guiData().m_showOpacityBlenderWindow = !m_appData.guiData().m_showOpacityBlenderWindow;
      break;
    case MainMenuAction::ResetPanelLayout:
      ImGui::ClearIniSettings();
      m_appData.guiData().m_showImagePropertiesWindow = true;
      m_appData.guiData().m_showSegmentationsWindow = true;
      m_applyDefaultPanelLayout = true;
      break;
    case MainMenuAction::ToggleImGuiDemoWindow:
      m_appData.guiData().m_showImGuiDemoWindow = !m_appData.guiData().m_showImGuiDemoWindow;
      break;
    case MainMenuAction::ToggleImPlotDemoWindow:
      m_appData.guiData().m_showImPlotDemoWindow = !m_appData.guiData().m_showImPlotDemoWindow;
      break;
    case MainMenuAction::ToggleToolbar:
      m_appData.guiData().m_showModeToolbar = !m_appData.guiData().m_showModeToolbar;
      if (m_readjustViewport) m_readjustViewport();
      break;
    case MainMenuAction::ResetProjectSettings:
      requestResetProjectSettings();
      break;
    case MainMenuAction::AddLandmark:
      addLandmarkAtCrosshairs();
      break;
  }
  if (m_postEmptyGlfwEvent) {
    m_postEmptyGlfwEvent();
  }
}
