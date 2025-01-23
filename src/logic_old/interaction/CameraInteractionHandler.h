#ifndef CAMERA_INTERACTION_HANDLER_H
#define CAMERA_INTERACTION_HANDLER_H

#include "logic/interaction/InteractionHandlerBase.h"
#include "logic/interaction/InteractionModes.h"

#include "common/PublicTypes.h"

#include <glm/vec3.hpp>

#include <functional>
#include <optional>
#include <unordered_map>

namespace camera
{
class Camera;
}

/**
 * @brief Handles pointer interactions that affect a view's camera.
 */
class CameraInteractionHandler : public InteractionHandlerBase
{
private:
  /// Function returning the World-space crosshairs origin position.
  using CrosshairsOriginProviderType = std::function<glm::vec3(void)>;

  /// Function returning the World-space center of the reference space's AABBox.
  using RefSpaceAABBoxCenterProviderType = std::function<glm::vec3(void)>;

  /// Function returning the World-space size of the reference space's AABBox.
  using RefSpaceAABBoxSizeProviderType = std::function<glm::vec3(void)>;

  /// Function returning the World-space diagonal voxel length of the reference space.
  using RefSpaceVoxelScaleProviderType = std::function<float(void)>;

  /// Function that synchronizes absolute zoom values among cameras linked to the camera of this
  /// interaction handler. The optional worldCenterPos argument is a World-space point to zoom towards
  /// in all synchronized views.
  using ZoomSynchronizer
    = std::function<void(float absoluteZoomValue, const std::optional<glm::vec3>& worldCenterPos)>;

public:
  explicit CameraInteractionHandler();
  ~CameraInteractionHandler() override = default;

  void setCameraProvider(GetterType<camera::Camera*> cameraProvider);
  void setCrosshairsOriginProvider(CrosshairsOriginProviderType);
  void setRefSpaceAABBoxCenterProvider(RefSpaceAABBoxCenterProviderType);
  void setRefSpaceAABBoxSizeProvider(RefSpaceAABBoxSizeProviderType);
  void setRefSpaceVoxelScaleProvider(RefSpaceVoxelScaleProviderType);
  void setZoomSynchronizer(ZoomSynchronizer);
  void setWorldCameraPositionBroadcaster(SetterType<glm::vec3>);

  void setMode(const CameraInteractionMode&);

private:
  enum class MouseMoveMode
  {
    Translate,
    TranslateFrontBack,
    RotateInPlane,
    RotateAboutCameraOrigin,
    RotateAboutCrosshairs,
    RotateAboutImageCenter,
    ZoomAboutPoint,
    ZoomAboutCenter,
    None
  };

  bool doHandleMouseDoubleClickEvent(const QMouseEvent*, const Viewport&, const camera::Camera&)
    override;
  bool doHandleMouseMoveEvent(const QMouseEvent*, const Viewport&, const camera::Camera&) override;
  bool doHandleMousePressEvent(const QMouseEvent*, const Viewport&, const camera::Camera&) override;
  bool doHandleMouseReleaseEvent(const QMouseEvent*, const Viewport&, const camera::Camera&) override;

  bool doHandleTabletEvent(const QTabletEvent*, const Viewport&, const camera::Camera&) override
  {
    return false;
  }

  bool doHandleWheelEvent(const QWheelEvent*, const Viewport&, const camera::Camera&) override;

  bool doHandlePanGesture(const QPanGesture*, const Viewport&, const camera::Camera&) override;
  bool doHandlePinchGesture(const QPinchGesture*, const Viewport&, const camera::Camera&) override;
  bool doHandleSwipeGesture(const QSwipeGesture*, const Viewport&, const camera::Camera&) override;
  bool doHandleTapGesture(const QTapGesture*, const Viewport&, const camera::Camera&) override
  {
    return false;
  }
  bool doHandleTapAndHoldGesture(const QTapAndHoldGesture*, const Viewport&, const camera::Camera&)
    override
  {
    return false;
  }

  GetterType<camera::Camera*> m_cameraProvider;
  CrosshairsOriginProviderType m_crosshairsOriginProvider;
  RefSpaceAABBoxCenterProviderType m_refSpaceCenterProvider;
  RefSpaceVoxelScaleProviderType m_refSpaceVoxelScaleProvider;
  RefSpaceAABBoxSizeProviderType m_refSpaceAABBoxSizeProvider;
  ZoomSynchronizer m_zoomSynchronizer;
  SetterType<glm::vec3> m_worldCameraPositionBroadcaster;

  CameraInteractionMode m_primaryMode;
  MouseMoveMode m_mouseMoveMode;

  glm::vec2 m_ndcLeftButtonStartPos;
  glm::vec2 m_ndcRightButtonStartPos;
  glm::vec2 m_ndcMiddleButtonStartPos;

  glm::vec2 m_ndcLeftButtonLastPos;
  glm::vec2 m_ndcRightButtonLastPos;
  glm::vec2 m_ndcMiddleButtonLastPos;

  const static std::unordered_map<CameraInteractionMode, MouseMoveMode> msk_defaultInternalModeMap;
};

#endif // CAMERA_INTERACTION_HANDLER_H
