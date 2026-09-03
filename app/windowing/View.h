#pragma once

#include "common/CoordinateFrame.h"
#include "common/IntersectionTypes.h"
#include "common/Types.h"
#include "logic/app/CrosshairsState.h"
#include "logic/camera/Camera.h"
#include "logic/camera/Camera3DControls.h"
#include "logic/camera/CameraTypes.h"
#include "rendering/utility/math/SliceIntersectorTypes.h"
#include "ui/UiControls.h"
#include "viewer/ViewModes.h"
#include "viewer/ViewTypes.h"
#include "windowing/ControlFrame.h"

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include <uuid.h>

#include <optional>

class Image;

/**
 * @brief Rendered viewport with cameras, slice state, 3D state, and sync-group membership
 */
class View : public ControlFrame
{
  using uuid = uuids::uuid;

public:
  /**
   * @brief Construct a view
   * @param winClipViewport Viewport bounds in enclosing-window clip coordinates
   * @param offsetSetting Slice offset behavior
   * @param viewType View orientation/type
   * @param renderMode Initial render mode
   * @param ipMode Initial intensity projection mode
   * @param uiControls UI controls enabled for this view
   * @param viewConvention View orientation convention
   * @param crosshairs Crosshairs state referenced by the view
   * @param viewAlignment View alignment mode
   * @param cameraRotationSyncGroupUid Rotation sync group, when any
   * @param translationSyncGroup Translation sync group, when any
   * @param zoomSyncGroup Zoom sync group, when any
   * @throw Propagates exceptions from camera construction or UID generation
   */
  View(
    glm::vec4 winClipViewport,
    ViewOffsetSetting offsetSetting,
    ViewType viewType,
    ViewRenderMode renderMode,
    IntensityProjectionMode ipMode,
    UiControls uiControls,
    const ViewConvention& viewConvention,
    const CrosshairsState& crosshairs,
    const ViewAlignmentMode& viewAlignment,
    std::optional<uuid> cameraRotationSyncGroupUid,
    std::optional<uuid> cameraTranslationSyncGroup,
    std::optional<uuid> cameraZoomSyncGroup);

  /**
   * @brief Get the stable view UID
   * @return View UID
   */
  const uuid& uid() const;

  /**
   * @brief Set the view type and reconcile camera projection and render mode
   * @param newViewType New view type
   * @throw Propagates exceptions from projection allocation
   */
  void setViewType(const ViewType& newViewType) override;

  /**
   * @brief Set the render mode if compatible with the current view type
   * @param renderMode Requested render mode
   */
  void setRenderMode(const ViewRenderMode& renderMode) override;

  /**
   * @brief Get the active camera for the current view type
   * @return 3D camera for 3D views; slice camera otherwise
   */
  const Camera& camera() const;

  /**
   * @brief Get the mutable active camera for the current view type
   * @return 3D camera for 3D views; slice camera otherwise
   */
  Camera& camera();

  /**
   * @brief Get the 2D slice camera
   * @return Const slice camera
   */
  const Camera& sliceCamera() const;

  /**
   * @brief Get the mutable 2D slice camera
   * @return Mutable slice camera
   */
  Camera& sliceCamera();

  /**
   * @brief Get the dedicated 3D camera
   * @return Const 3D camera
   */
  const Camera& threeDCamera() const;

  /**
   * @brief Get the mutable dedicated 3D camera
   * @return Mutable 3D camera
   */
  Camera& threeDCamera();

  /**
   * @brief Get the dedicated 3D camera interaction state
   * @return Const 3D camera state
   */
  const camera3d::State& threeDState() const;
  /**
   * @brief Get the mutable dedicated 3D camera interaction state
   * @return Mutable 3D camera state
   */
  camera3d::State& threeDState();

  /** @brief Return whether the dedicated 3D camera has been framed for a scene. */
  bool isThreeDCameraInitialized() const;

  /**
   * @brief Restore a previously captured dedicated 3D camera and interaction state
   * @param cameraArg Camera snapshot to restore
   * @param state Interaction-state snapshot to restore
   * @param initialized Whether the snapshot had already been framed for a scene
   */
  void restoreThreeDCamera(const Camera& cameraArg, const camera3d::State& state, bool initialized);

  /**
   * @brief Set the projection type of the dedicated 3D camera
   * @param projectionType Projection type to activate
   * @throw Propagates exceptions from projection allocation
   */
  void setThreeDProjectionType(ProjectionType projectionType);
  /**
   * @brief Initialize the dedicated 3D camera to its default pose if it has not been initialized
   * @param scene Scene frame used for camera placement
   * @throw Propagates exceptions from camera projection access
   */
  void initializeThreeDCameraIfNeeded(const camera3d::SceneFrame& scene);
  /**
   * @brief Recenter the dedicated 3D camera around a target
   * @param scene Scene frame used for distances and clipping
   * @param target World-space target
   * @throw Propagates exceptions from camera projection access
   */
  void recenterThreeDCamera(const camera3d::SceneFrame& scene, const glm::vec3& target);
  /**
   * @brief Reset the dedicated 3D camera pose and projection scale
   * @param scene Scene frame used for default placement
   * @param target World-space target for the reset view
   * @throw Propagates exceptions from projection allocation
   */
  void resetThreeDCamera(const camera3d::SceneFrame& scene, const glm::vec3& target);

  /**
   * @brief Recenter the dedicated 2D slice camera and remember the default slice-camera frame
   * @param worldCenter Target center in World space
   * @param worldFov Field of view size in World units
   * @param resetZoom True to reset camera zoom and default field of view
   * @param resetObliqueOrientation True to reset oblique view orientation
   * @throw Propagates exceptions from camera transforms
   */
  void recenterSliceCamera(
    const glm::vec3& worldCenter,
    const glm::vec3& worldFov,
    bool resetZoom,
    bool resetObliqueOrientation);

  /**
   * @brief Update the view's camera based on the crosshairs World-space position
   * @param appData Application data used for image geometry
   * @param worldCrosshairs Crosshairs position in world coordinates
   * @return Crosshairs position on the current slice
   * @throw Propagates exceptions from image, camera, or transformation access
   */
  glm::vec3 updateImageSlice(const AppData& appData, const glm::vec3& worldCrosshairs);

  /**
   * @brief Compute this view's intersection polygon with one image
   * @param image Image to intersect
   * @param crosshairs Crosshairs frame used to position the slice
   * @return Intersection vertices, or std::nullopt when the slice misses the image
   * @throw Propagates exceptions from image or camera transform access
   */
  std::optional<intersection::IntersectionVerticesVec4> computeImageSliceIntersection(
    const Image* image,
    const CoordinateFrame& crosshairs) const;

  /**
   * @brief Get the clip-space depth of the current image plane
   * @return Current image-plane depth in clip coordinates
   */
  float clipPlaneDepth() const;

  /**
   * @brief Get slice offset behavior for this view
   * @return Slice offset setting
   */
  const ViewOffsetSetting& offsetSetting() const;

  /**
   * @brief Get the camera rotation sync group UID
   * @return Rotation sync group UID, or `std::nullopt`
   */
  std::optional<uuid> cameraRotationSyncGroupUid() const;

  /**
   * @brief Get the camera translation sync group UID
   * @return Translation sync group UID, or `std::nullopt`
   */
  std::optional<uuid> cameraTranslationSyncGroupUid() const;

  /**
   * @brief Get the camera zoom sync group UID
   * @return Zoom sync group UID, or `std::nullopt`
   */
  std::optional<uuid> cameraZoomSyncGroupUid() const;

private:
  /**
   * @brief Compute the anatomical start frame for a view type
   * @param viewType View type whose start frame should be computed
   * @return Anatomical start coordinate frame
   * @throw Propagates exceptions from map lookup if the view type or convention is unsupported
   */
  CoordinateFrame get_anatomy_T_start(const ViewType& viewType) const;

  /**
   * @brief Update cached image slice intersection data for rendering
   * @param appData Application data containing images
   * @param worldCrosshairs Crosshairs position in World space
   * @return True when slice intersection data was updated
   * @throw Propagates exceptions from image or camera transform access
   */
  bool updateImageSliceIntersection(const AppData& appData, const glm::vec3& worldCrosshairs);

  /** @brief Stable view UID */
  const uuid m_uid;

  /** @brief Slice offset behavior */
  ViewOffsetSetting m_offset;

  /** @brief Projection type used by the 2D slice camera */
  ProjectionType m_projectionType;
  /** @brief Dedicated camera for 2D slice views */
  Camera m_camera;
  /** @brief Dedicated camera for the 3D view type */
  Camera m_threeDCamera;
  /** @brief Interaction state for the dedicated 3D camera */
  camera3d::State m_threeDState;
  /** @brief True after the 3D camera has been initialized for a scene */
  bool m_threeDCameraInitialized = false;
  /** @brief True after the dedicated 2D camera has been used as the active view camera */
  bool m_sliceCameraActivated = false;
  /** @brief Last default recenter target for the dedicated 2D camera */
  std::optional<glm::vec3> m_sliceCameraDefaultWorldCenter = std::nullopt;
  /** @brief Last default field of view for the dedicated 2D camera */
  std::optional<glm::vec3> m_sliceCameraDefaultWorldFov = std::nullopt;

  /** @brief Most recently selected render mode for a 2D view type */
  ViewRenderMode m_last2dRenderMode = ViewRenderMode::Image;
  /** @brief Most recently selected render mode for the 3D view type */
  ViewRenderMode m_last3dRenderMode = ViewRenderMode::SegmentationAndIsosurfaces;

  /** @brief Referenced application-level view convention */
  const ViewConvention& m_viewConvention;
  /** @brief Referenced crosshairs state used to position view slices */
  const CrosshairsState& m_crosshairs;
  /** @brief Referenced application-level view alignment mode */
  const ViewAlignmentMode& m_viewAlignment;

  /** @brief Rotation sync group UID, when any */
  std::optional<uuid> m_cameraRotationSyncGroupUid;
  /** @brief Translation sync group UID, when any */
  std::optional<uuid> m_cameraTranslationSyncGroupUid;
  /** @brief Zoom sync group UID, when any */
  std::optional<uuid> m_cameraZoomSyncGroupUid;

  /** @brief Current image-plane depth in clip coordinates */
  float m_clipPlaneDepth;

  /** @brief Cached anatomical start frame used by 2D view cameras */
  CoordinateFrame m_anatomy_T_start;
};
