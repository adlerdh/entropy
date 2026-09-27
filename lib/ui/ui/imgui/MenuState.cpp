#include "ui/ImGuiWrapper.h"

#include "logic/app/CallbackHandler.h"
#include "logic/app/Data.h"
#include "ui/imgui/ImageSelection.h"
#include "ui/imgui/TimePlayback.h"
#include "ui/menus/MainMenuBar.h"

#include <spdlog/fmt/std.h>
#include <spdlog/spdlog.h>

#include <functional>
#include <utility>

using namespace ui::imgui_detail;

bool ImGuiWrapper::isMenuActionEnabled(MainMenuAction action)
{
  const bool loaded = ProjectLoadState::Loaded == m_appData.state().projectLoadState();
  const bool backgroundTaskRunning = m_appData.state().animating();
  const bool canUseProjectActions = loaded && !backgroundTaskRunning;
  const bool hasActiveImage = activeImageUid().has_value();
  const bool hasActiveSeg = activeSegUid().has_value();
  const auto [annotImageUid, annotUid] = activeAnnotation();
  const bool hasActiveAnnotation = annotImageUid.has_value() && annotUid.has_value();

  switch (action) {
    case MainMenuAction::SetModePointer:
    case MainMenuAction::SetModeWindowLevel:
    case MainMenuAction::SetModeZoom:
    case MainMenuAction::SetModePan:
    case MainMenuAction::SetModeRotateView:
    case MainMenuAction::SetModeRotateCrosshairs:
    case MainMenuAction::SetModeSegment:
    case MainMenuAction::SetModeAnnotate:
    case MainMenuAction::SetModeTranslateImage:
    case MainMenuAction::SetModeRotateImage:
    case MainMenuAction::SetModeScaleImage:
    case MainMenuAction::Recenter:
    case MainMenuAction::ResetView:
    case MainMenuAction::ToggleImageVisibility:
    case MainMenuAction::ToggleImageEdges:
    case MainMenuAction::DecreaseActiveImageOpacity:
    case MainMenuAction::IncreaseActiveImageOpacity:
    case MainMenuAction::ToggleCrosshairsVoxelSnapping:
    case MainMenuAction::ToggleCrosshairs:
    case MainMenuAction::ToggleScaleBars:
    case MainMenuAction::ToggleAsciiRendering:
    case MainMenuAction::ToggleLightboxOffsets:
    case MainMenuAction::CycleViewOverlays:
    case MainMenuAction::ToggleUserInterface:
    case MainMenuAction::ToggleEntropyInstanceSync:
    case MainMenuAction::ToggleSync:
    case MainMenuAction::ToggleSyncSendCursor:
    case MainMenuAction::ToggleSyncReceiveCursor:
    case MainMenuAction::ToggleSyncSendZoom:
    case MainMenuAction::ToggleSyncReceiveZoom:
    case MainMenuAction::ToggleSyncSendPan:
    case MainMenuAction::ToggleSyncReceivePan:
    case MainMenuAction::ShowOpacityMixer:
    case MainMenuAction::FirstTimePoint:
    case MainMenuAction::PreviousTimePoint:
    case MainMenuAction::NextTimePoint:
    case MainMenuAction::LastTimePoint:
    case MainMenuAction::AddIsosurface:
    case MainMenuAction::AddIsosurfaceRange:
    case MainMenuAction::ImportSurfaceMesh:
    case MainMenuAction::CreateSegmentation:
    case MainMenuAction::CreateLandmarkGroup:
    case MainMenuAction::AddLayout:
      if (
        action == MainMenuAction::FirstTimePoint || action == MainMenuAction::PreviousTimePoint ||
        action == MainMenuAction::NextTimePoint || action == MainMenuAction::LastTimePoint)
      {
        return loaded && globalTimeControlImageUid(m_appData).has_value();
      }
      return canUseProjectActions && hasActiveImage;
    case MainMenuAction::ToggleGlobalTimeControls:
      return loaded;
    case MainMenuAction::ToggleTimePlayback:
      return loaded && globalTimeControlImageUid(m_appData).has_value();
    case MainMenuAction::ToggleLayoutTabs:
    case MainMenuAction::ResetProjectSettings:
      return canUseProjectActions;
    case MainMenuAction::ActivatePreviousImage:
    case MainMenuAction::ActivateNextImage:
      return canUseProjectActions && hasActiveImage && m_appData.numImages() > 1;
    case MainMenuAction::PreviousForegroundLabel:
    case MainMenuAction::NextForegroundLabel:
    case MainMenuAction::PreviousBackgroundLabel:
    case MainMenuAction::NextBackgroundLabel:
    case MainMenuAction::DecreaseBrushSize:
    case MainMenuAction::IncreaseBrushSize:
      return canUseProjectActions && hasActiveSeg;
    case MainMenuAction::ToggleFullScreen:
    case MainMenuAction::ToggleSynchronizeThreeDCameras:
    case MainMenuAction::ToggleImagesWindow:
    case MainMenuAction::ToggleSegmentationsWindow:
    case MainMenuAction::ToggleRegionStatisticsWindow:
    case MainMenuAction::ToggleLandmarksWindow:
    case MainMenuAction::ToggleAnnotationsWindow:
    case MainMenuAction::ToggleIsosurfacesWindow:
    case MainMenuAction::ToggleSettingsWindow:
    case MainMenuAction::ShowSettingsWindow:
    case MainMenuAction::ShowSynchronizeSettingsWindow:
    case MainMenuAction::ToggleInspectorWindow:
    case MainMenuAction::ToggleOpacityMixerWindow:
    case MainMenuAction::ResetPanelLayout:
    case MainMenuAction::ToggleImGuiDemoWindow:
    case MainMenuAction::ToggleImPlotDemoWindow:
    case MainMenuAction::ToggleToolbar:
      return !backgroundTaskRunning;
    case MainMenuAction::ToggleSegmentationVisibility:
    case MainMenuAction::ToggleSegmentationOutline:
    case MainMenuAction::DecreaseSegmentationOpacity:
    case MainMenuAction::IncreaseSegmentationOpacity:
    case MainMenuAction::ExportActiveSegmentation:
    case MainMenuAction::ClearSegmentation:
      return canUseProjectActions && hasActiveSeg;
    case MainMenuAction::RemoveSegmentation:
      if (!canUseProjectActions || !hasActiveSeg || !hasActiveImage) return false;
      return m_appData.imageToSegUids(*activeImageUid()).size() > 1;
    case MainMenuAction::ExportActiveImage:
      if (!canUseProjectActions || !hasActiveImage) return false;
      if (const Image* image = m_appData.image(*activeImageUid())) return image->hasPixelData();
      return false;
    case MainMenuAction::SetActiveImageAsReference:
      return canUseProjectActions && hasActiveImage && activeImageUid() != m_appData.refImageUid();
    case MainMenuAction::RemoveActiveImage:
    case MainMenuAction::MoveActiveImageBackward:
    case MainMenuAction::MoveActiveImageForward:
    case MainMenuAction::MoveActiveImageToBack:
    case MainMenuAction::MoveActiveImageToFront:
      return canUseProjectActions && hasActiveImage;
    case MainMenuAction::ToggleActiveImageTransformationLock:
      if (!canUseProjectActions || !hasActiveImage || !m_setLockManualImageTransformation) return false;
      return activeImageUid() != m_appData.refImageUid();
    case MainMenuAction::LoadActiveImageInitialTransformation:
    case MainMenuAction::SaveActiveImageInitialTransformation:
    case MainMenuAction::ResetActiveImageInitialTransformation:
    case MainMenuAction::ResetActiveImageManualTransformation:
    case MainMenuAction::SaveActiveImageManualTransformation:
      return canUseProjectActions && hasActiveImage && activeImageUid() != m_appData.refImageUid();
    case MainMenuAction::SaveActiveImageEffectiveTransformation:
      if (!canUseProjectActions || !hasActiveImage || activeImageUid() == m_appData.refImageUid()) return false;
      if (const Image* image = m_appData.image(*activeImageUid())) {
        return image->transformations().get_enable_affine_T_subject();
      }
      return false;
    case MainMenuAction::ToggleApplyActiveImageWarp:
      return canUseProjectActions && hasActiveImage &&
             (activeImageUid() != m_appData.refImageUid() || imageIsOnlyNonWarpImage(m_appData, *activeImageUid())) &&
             m_appData.imageToActiveInverseWarpUid(*activeImageUid()).has_value();
    case MainMenuAction::ShowRegistrationSetupWindow:
    case MainMenuAction::ToggleRegistrationJobsWindow:
      return canUseProjectActions;
    case MainMenuAction::PaintSegmentationFromAnnotation:
      return canUseProjectActions && hasActiveSeg && hasActiveAnnotation;
    case MainMenuAction::ImportAnnotations:
      return canUseProjectActions && activeImageUid().has_value();
    case MainMenuAction::ExportAnnotations:
      return canUseProjectActions && activeImageHasAnnotations();
    case MainMenuAction::RemoveAnnotation:
    case MainMenuAction::MoveAnnotationBackward:
    case MainMenuAction::MoveAnnotationForward:
    case MainMenuAction::MoveAnnotationToBack:
    case MainMenuAction::MoveAnnotationToFront:
      return canUseProjectActions && hasActiveAnnotation;
    case MainMenuAction::ImportLandmarkGroup:
      return canUseProjectActions && activeImageUid().has_value();
    case MainMenuAction::SaveLandmarkGroup:
    case MainMenuAction::RemoveLandmarkGroup:
      return canUseProjectActions && activeLandmarkGroupUid().has_value();
    case MainMenuAction::RemoveLayout:
      return canUseProjectActions && m_appData.windowData().numLayouts() >= 2;
    case MainMenuAction::AddLandmark:
      return canUseProjectActions && activeLandmarkGroupUid().has_value();
  }
  return false;
}

bool ImGuiWrapper::isMenuActionChecked(MainMenuAction action)
{
  switch (action) {
    case MainMenuAction::SetModePointer:
      return MouseMode::Pointer == m_appData.state().mouseMode();
    case MainMenuAction::SetModeWindowLevel:
      return MouseMode::WindowLevel == m_appData.state().mouseMode();
    case MainMenuAction::SetModeZoom:
      return MouseMode::CameraZoom == m_appData.state().mouseMode();
    case MainMenuAction::SetModePan:
      return MouseMode::CameraTranslate == m_appData.state().mouseMode();
    case MainMenuAction::SetModeRotateView:
      return MouseMode::CameraRotate == m_appData.state().mouseMode();
    case MainMenuAction::SetModeRotateCrosshairs:
      return MouseMode::CrosshairsRotate == m_appData.state().mouseMode();
    case MainMenuAction::SetModeSegment:
      return MouseMode::Segment == m_appData.state().mouseMode();
    case MainMenuAction::SetModeAnnotate:
      return MouseMode::Annotate == m_appData.state().mouseMode();
    case MainMenuAction::SetModeTranslateImage:
      return MouseMode::ImageTranslate == m_appData.state().mouseMode();
    case MainMenuAction::SetModeRotateImage:
      return MouseMode::ImageRotate == m_appData.state().mouseMode();
    case MainMenuAction::SetModeScaleImage:
      return MouseMode::ImageScale == m_appData.state().mouseMode();
    case MainMenuAction::ToggleImageVisibility: {
      const auto imageUid = m_appData.activeImageUid();
      const Image* image = imageUid ? m_appData.image(*imageUid) : nullptr;
      if (!image) {
        return false;
      }
      const bool isMulticomponentImage = image->header().numComponentsPerPixel() > 1;
      return isMulticomponentImage ? image->settings().globalVisibility() : image->settings().visibility();
    }
    case MainMenuAction::ToggleSegmentationVisibility: {
      const auto imageUid = m_appData.activeImageUid();
      const auto segUid = imageUid ? m_appData.imageToActiveSegUid(*imageUid) : std::nullopt;
      const Image* seg = segUid ? m_appData.seg(*segUid) : nullptr;
      return seg ? seg->settings().visibility() : false;
    }
    case MainMenuAction::ToggleImageEdges: {
      const auto imageUid = m_appData.activeImageUid();
      const Image* image = imageUid ? m_appData.image(*imageUid) : nullptr;
      return image ? image->settings().edgesVisible() : false;
    }
    case MainMenuAction::ToggleSegmentationOutline:
      return SegmentationOutlineStyle::Disabled != m_appData.renderSettings().m_segOutlineStyle;
    case MainMenuAction::ToggleSynchronizeThreeDCameras:
      return m_appData.renderSettings().m_synchronizeThreeDCameras;
    case MainMenuAction::ToggleActiveImageTransformationLock: {
      const auto imageUid = m_appData.activeImageUid();
      const Image* image = imageUid ? m_appData.image(*imageUid) : nullptr;
      return image ? image->transformations().is_worldDef_T_affine_locked() : false;
    }
    case MainMenuAction::ToggleApplyActiveImageWarp: {
      const auto imageUid = m_appData.activeImageUid();
      const Image* image = imageUid ? m_appData.image(*imageUid) : nullptr;
      return image ? image->settings().warpEnabled() : false;
    }
    case MainMenuAction::ToggleRegistrationJobsWindow:
      return m_appData.guiData().m_showRegistrationJobsWindow;
    case MainMenuAction::ToggleScaleBars:
      return m_appData.renderSettings().m_showScaleBars;
    case MainMenuAction::ToggleCrosshairs:
      return m_appData.renderSettings().m_showCrosshairs;
    case MainMenuAction::ToggleCrosshairsVoxelSnapping:
      return CrosshairsSnapping::Disabled != m_appData.renderSettings().m_snapCrosshairs;
    case MainMenuAction::ToggleAsciiRendering:
      return m_appData.renderSettings().m_asciiEnabled;
    case MainMenuAction::ToggleLightboxOffsets:
      return m_appData.renderSettings().m_showLightboxOffsetLabels;
    case MainMenuAction::ToggleUserInterface:
      return m_callbackHandler.showUserInterface();
    case MainMenuAction::ToggleLayoutTabs:
      return m_appData.settings().showLayoutTabs();
    case MainMenuAction::ToggleGlobalTimeControls:
      return m_appData.settings().showGlobalTimeControls();
    case MainMenuAction::ToggleEntropyInstanceSync:
      return m_appData.settings().entropyInstanceSyncEnabled();
    case MainMenuAction::ToggleSync:
      return m_appData.settings().cursorSyncEnabled();
    case MainMenuAction::ToggleSyncSendCursor:
      return m_appData.settings().sendCursorSync();
    case MainMenuAction::ToggleSyncReceiveCursor:
      return m_appData.settings().receiveCursorSync();
    case MainMenuAction::ToggleSyncSendZoom:
      return m_appData.settings().sendZoomSync();
    case MainMenuAction::ToggleSyncReceiveZoom:
      return m_appData.settings().receiveZoomSync();
    case MainMenuAction::ToggleSyncSendPan:
      return m_appData.settings().sendPanSync();
    case MainMenuAction::ToggleSyncReceivePan:
      return m_appData.settings().receivePanSync();
    case MainMenuAction::ToggleToolbar:
      return m_appData.guiData().m_showModeToolbar;
    case MainMenuAction::ToggleImagesWindow:
      return m_appData.guiData().m_showImagePropertiesWindow;
    case MainMenuAction::ToggleSegmentationsWindow:
      return m_appData.guiData().m_showSegmentationsWindow;
    case MainMenuAction::ToggleRegionStatisticsWindow:
      return m_appData.guiData().m_showRegionStatisticsWindow;
    case MainMenuAction::ToggleLandmarksWindow:
      return m_appData.guiData().m_showLandmarksWindow;
    case MainMenuAction::ToggleAnnotationsWindow:
      return m_appData.guiData().m_showAnnotationsWindow;
    case MainMenuAction::ToggleIsosurfacesWindow:
      return m_appData.guiData().m_showIsosurfacesWindow;
    case MainMenuAction::ToggleSettingsWindow:
      return m_appData.guiData().m_showSettingsWindow;
    case MainMenuAction::ToggleInspectorWindow:
      return m_appData.guiData().m_showInspectionWindow;
    case MainMenuAction::ToggleOpacityMixerWindow:
      return m_appData.guiData().m_showOpacityBlenderWindow;
    case MainMenuAction::ShowRegistrationSetupWindow:
      return m_appData.guiData().m_showRegistrationSetupWindow;
    case MainMenuAction::ShowOpacityMixer:
      return m_appData.guiData().m_showOpacityBlenderWindow;
    case MainMenuAction::ToggleTimePlayback: {
      const auto imageUid = globalTimeControlImageUid(m_appData);
      const Image* image = imageUid ? m_appData.image(*imageUid) : nullptr;
      return image ? image->settings().timePlaybackPlaying() : false;
    }
    case MainMenuAction::ToggleImGuiDemoWindow:
      return m_appData.guiData().m_showImGuiDemoWindow;
    case MainMenuAction::ToggleImPlotDemoWindow:
      return m_appData.guiData().m_showImPlotDemoWindow;
    default:
      return false;
  }
}
