#pragma once

#include "common/SegmentationTypes.h"
#include "common/Types.h"
#include "logic/app/ImageScaleInteraction.h"
#include "logic/interaction/ViewHit.h"

#include <glm/fwd.hpp>
#include <uuid.h>

#include <cstdint>
#include <optional>
#include <set>
#include <vector>

class AppData;
class GlfwWrapper;
class Rendering;
class View;

/**
 * @brief Handles UI callbacks to the application
 */
class CallbackHandler
{
  using uuid = uuids::uuid;

public:
  CallbackHandler(AppData&, GlfwWrapper&, Rendering&);
  ~CallbackHandler() = default;

  /**
   * @brief Clears all voxels in a segmentation, setting them to 0
   * @param segUid
   * @return
   */
  bool clearSegVoxels(const uuid& segUid);

  /// Create a blank multi-component image with the same header as the given image
  std::optional<uuid> createBlankImageAndTexture(
    const uuid& matchImageUid,
    const ComponentType& componentType,
    uint32_t numComponents,
    const std::string& displayName,
    bool createSegmentation);

  /// Create a blank segmentation with the same header as the given image
  /// Does not create texture
  std::optional<uuid> createBlankSeg(const uuid& matchImageUid, const std::string& displayName);

  /// Create a blank segmentation with the same header as the given image
  std::optional<uuid> createBlankSegWithColorTableAndTextures(
    const uuid& matchImageUid,
    const std::string& displayName);

  /// Assign an existing segmentation to an image and create any rendering resources it needs
  std::optional<uuid> assignSegToImageWithColorTableAndTextures(
    const uuid& matchImageUid,
    const uuid& segUid,
    bool createLabelColorTable,
    bool removeSegOnFailure);

  bool executePoissonSegmentation(const uuid& imageUid, const uuid& seedSegUid, const SeedSegmentationType& segType);

  /**
   * @brief Move the crosshairs
   * @param windowLastPos
   * @param windowCurrPos
   */
  void doCrosshairsMove(const ViewHit& hit);

  /**
   * @brief Scroll the crosshairs
   * @param windowCurrPos
   * @param scrollOffset
   */
  void doCrosshairsScroll(const ViewHit& hit, const glm::vec2& scrollOffset, bool fineScroll);

  /**
   * @brief Segment the image
   * @param windowLastPos
   * @param windowCurrPos
   * @param leftButton
   */
  void doSegment(const ViewHit& hit, bool swapFgAndBg);

  void updateBrushPreview(const ViewHit& hit, bool swapFgAndBg, bool paintingPreview);
  void refreshBrushPreviewIfNeeded();
  void clearBrushPreview();

  /**
   * @brief Paint the active segmentation of the active image with the
   * filled active annotation polygon. Do all of this in the annotation plane.
   */
  void paintActiveSegmentationWithAnnotation();

  /**
   * @brief Adjust image window/level
   * @param windowLastPos
   * @param windowCurrPos
   */
  void doWindowLevel(const ViewHit& startHit, const ViewHit& prevHit, const ViewHit& currHit, bool fineAdjustment);

  /**
   * @brief Adjust image opacity
   * @param windowLastPos
   * @param windowCurrPos
   */
  void doOpacity(const ViewHit& prevHit, const ViewHit& currHit);

  /**
   * @brief 2D translation of the camera (panning)
   * @param windowLastPos
   * @param windowCurrPos
   * @param windowStartPos
   */
  void doCameraTranslate2d(const ViewHit& startHit, const ViewHit& prevHit, const ViewHit& currHit);

  /**
   * @brief 2D rotation of the camera
   * @param[in] windowLastPos
   * @param[in] windowCurrPos
   * @param[in] windowStartPos
   * @param[in] rotationOrigin
   */
  void doCameraRotate2d(
    const ViewHit& startHit,
    const ViewHit& prevHit,
    const ViewHit& currHit,
    const RotationOrigin& rotationOrigin);

  /**
   * @brief 3D rotation of the camera
   * @param[in] windowLastPos
   * @param[in] windowCurrPos
   * @param[in] windowStartPos
   * @param[in] rotationOrigin
   * @param[in] constraint
   */
  void doCameraRotate3d(
    const ViewHit& startHit,
    const ViewHit& prevHit,
    const ViewHit& currHit,
    const RotationOrigin& rotationOrigin,
    const AxisConstraint& constraint);

  void doThreeDCameraOrbit(const ViewHit& startHit, const ViewHit& prevHit, const ViewHit& currHit);
  void doThreeDCameraRotateAboutEye(const ViewHit& startHit, const ViewHit& prevHit, const ViewHit& currHit);
  void doThreeDCameraRoll(const ViewHit& startHit, const ViewHit& prevHit, const ViewHit& currHit);
  void doThreeDCameraPan(const ViewHit& startHit, const ViewHit& prevHit, const ViewHit& currHit);
  void doThreeDCameraScroll(const ViewHit& hit, const glm::vec2& scrollOffset, bool faster, bool adjustPerspectiveFov);
  void doThreeDCameraKeyboardPanOrRotate(const ViewHit& hit, const glm::ivec2& direction, bool rotate, bool faster);
  void doThreeDIsosurfacePick(const ViewHit& hit);

  /**
   * @brief 3D rotation of the camera
   * @param viewUid
   * @param camera_T_world_rotationDelta
   */
  void doCameraRotate3d(const uuid& viewUid, const glm::quat& camera_T_world_rotationDelta);

  /**
   * @brief Set the forward direction of a view and synchronize with its linked views
   * @param worldForwardDirection
   */
  void handleSetViewForwardDirection(const uuid& viewUid, const glm::vec3& worldForwardDirection);

  /**
   * @brief 2D zoom of the camera
   * @param windowLastPos
   * @param windowCurrPos
   * @param windowStartPos
   * @param zoomBehavior
   * @param syncZoomForAllViews
   */
  void doCameraZoomDrag(
    const ViewHit& startHit,
    const ViewHit& prevHit,
    const ViewHit& currHit,
    const ZoomBehavior& zoomBehavior,
    bool syncZoomForAllViews);

  /**
   * @brief doCameraZoomScroll
   * @param scrollOffset
   * @param windowStartPos
   * @param zoomBehavior
   * @param syncZoomForAllViews
   */
  void doCameraZoomScroll(
    const ViewHit& hit,
    const glm::vec2& scrollOffset,
    const ZoomBehavior& zoomBehavior,
    bool syncZoomForAllViews);

  /**
   * @brief Image rotation
   * @param windowLastPos
   * @param windowCurrPos
   * @param windowStartPos
   * @param inPlane
   */
  void doImageRotate(const ViewHit& startHit, const ViewHit& prevHit, const ViewHit& currHit, bool inPlane);

  /**
   * @brief Image translation
   * @param windowLastPos
   * @param windowCurrPos
   * @param windowStartPos
   * @param inPlane
   */
  void doImageTranslate(const ViewHit& startHit, const ViewHit& prevHit, const ViewHit& currHit, bool inPlane);

  /**
   * @brief Image scale
   * @param windowLastPos
   * @param windowCurrPos
   * @param windowStartPos
   * @param constraint Scale constraint applied to the drag.
   */
  void doImageScale(
    const ViewHit& startHit,
    const ViewHit& prevHit,
    const ViewHit& currHit,
    entropy::app::ImageScaleConstraint constraint);

  /**
   * @brief scrollViewSlice
   * @param windowCurrPos
   * @param numSlices
   */
  void scrollViewSlice(const ViewHit& hit, int numSlices);

  /**
   * @brief moveCrosshairsOnViewSlice
   * @param windowCurrPos
   * @param stepX
   * @param stepY
   */
  void moveCrosshairsOnViewSlice(const ViewHit& hit, int stepX, int stepY);

  /**
   * @brief Rotate the crosshairs frame in the 2D view plane about the crosshairs position
   * @param[in] windowLastPos
   * @param[in] windowCurrPos
   * @param[in] windowStartPos
   */
  void
  doCrosshairsRotate2D(const ViewHit& startHit, const ViewHit& prevHit, const ViewHit& currHit, bool snapCrosshairs);

  /// @brief When the mouse is released, transition to a state where crosshairs are not rotating
  /// @todo Could be handled by dedicated state machine for crosshairs
  void endCrosshairsRotate2D();

  void moveCrosshairsToSegLabelCentroid(const uuid& imageUid, size_t labelIndex);

  /**
   * @brief Recenter all views on the selected images. Optionally recenter crosshairs there too.
   * @param recenterCrosshairs
   * @param recenterOnCurrentCrosshairsPos
   * @param resetObliqueOrientation
   */
  void recenterViews(
    const ImageSelection&,
    bool recenterCrosshairs,
    bool realignCrosshairs,
    bool recenterOnCurrentCrosshairsPos,
    bool resetObliqueOrientation,
    bool resetZoom,
    const std::set<uuid>& excludedViews = {});

  /**
   * @brief Recenter one view
   * @param viewUid
   */
  void recenterView(const ImageSelection&, const uuid& viewUid);

  void flipImageInterpolation();
  void toggleImageVisibility();
  void toggleImageEdges();

  void changeImageOpacity(double delta);
  void changeSegOpacity(double delta, bool interior);
  void toggleSegVisibility();
  void toggleSegGlobalOutline();

  void cyclePrevLayout();
  void cycleNextLayout();

  void cycleOverlayAndUiVisibility();

  void cycleImageComponent(int i);
  void cycleActiveImage(int i);

  /**
   * @brief Move the active time-series image by a relative frame offset.
   * @param i Relative frame offset.
   */
  void cycleTimePoint(int i);

  /**
   * @brief Toggle playback for the active time-series image.
   */
  void toggleTimePlayback();

  /**
   * @brief Move the active time-series image to its first frame.
   */
  void setTimePointToFirst();

  /**
   * @brief Move the active time-series image to its last frame.
   */
  void setTimePointToLast();

  void cycleForegroundSegLabel(int i);
  void cycleBackgroundSegLabel(int i);

  void cycleBrushSize(int i);

  bool showOverlays() const;
  void setShowOverlays(bool);
  bool showUserInterface() const;
  void setShowUserInterface(bool show);
  void toggleCrosshairs();
  void cycleViewOverlays();

  void setMouseMode(MouseMode);

  void toggleFullScreenMode(bool forceWindowMode = false);

  /// Set whether manual transformation are locked on an image and all of its segmentations
  bool setLockManualImageTransformation(const uuid& imageUid, bool locked);

  /// Synchronize the lock on an image to another image
  bool syncManualImageTransformation(const uuid& refImageUid, const uuid& otherImageUid);

  /// Synchronize the lock on all segmentations of the image
  bool syncManualImageTransformationOnSegs(const uuid& imageUid);

private:
  AppData& m_appData;
  GlfwWrapper& m_glfw;
  Rendering& m_rendering;
  std::optional<ViewHit> m_lastBrushPreviewHit;
  bool m_lastBrushPreviewSwapFgAndBg = false;
  bool m_lastBrushPreviewPainting = false;
  uint64_t m_lastBrushPreviewRevision = 0;
  std::vector<int64_t> m_brushPreviewData;

  /**
   * @brief This function is intended to run prior to cursor callbacks that require an active view.
   * If there is an active view and the active is NOT equal to the given view UID, then return
   * false. Otherwise, set the given view as active and return true. For callbacks that require an
   * active view, the returned false flag indicates that the callback should NOT proceed.
   *
   * @param[in] viewUid View UID to check against the active UID.
   */
  bool checkAndSetActiveView(const uuid& viewUid);

  /// Move any 3D view whose camera eye follows the global crosshairs.
  void updateThreeDViewsFollowingCrosshairs();
};
