#include "logic/interaction/CameraInteractionHandler.h"
#include "logic/camera/Camera.h"
#include "logic/camera/CameraHelpers.h"

#include <glm/glm.hpp>

const std::unordered_map<CameraInteractionMode, CameraInteractionHandler::MouseMoveMode>
  CameraInteractionHandler::msk_defaultInternalModeMap = {
    {CameraInteractionMode::Translate, CameraInteractionHandler::MouseMoveMode::Translate},
    {CameraInteractionMode::Rotate, CameraInteractionHandler::MouseMoveMode::RotateAboutImageCenter},
    {CameraInteractionMode::Zoom, CameraInteractionHandler::MouseMoveMode::ZoomAboutPoint}
};

CameraInteractionHandler::CameraInteractionHandler()
  : InteractionHandlerBase(InteractionHandlerType::Camera)
  ,

  m_cameraProvider(nullptr)
  , m_crosshairsOriginProvider(nullptr)
  , m_refSpaceCenterProvider(nullptr)
  , m_refSpaceVoxelScaleProvider(nullptr)
  , m_refSpaceAABBoxSizeProvider(nullptr)
  , m_zoomSynchronizer(nullptr)
  , m_worldCameraPositionBroadcaster(nullptr)
  ,

  m_primaryMode(CameraInteractionMode::Rotate)
  , m_ndcLeftButtonStartPos(0.0f)
  , m_ndcRightButtonStartPos(0.0f)
  , m_ndcMiddleButtonStartPos(0.0f)
  , m_ndcLeftButtonLastPos(0.0f)
  , m_ndcRightButtonLastPos(0.0f)
  , m_ndcMiddleButtonLastPos(0.0f)
{
  setUpdatesViewsOnEventHandled(true);
  m_mouseMoveMode = msk_defaultInternalModeMap.at(m_primaryMode);
}

void CameraInteractionHandler::setCameraProvider(GetterType<camera::Camera*> provider)
{
  m_cameraProvider = provider;
}

void CameraInteractionHandler::setCrosshairsOriginProvider(CrosshairsOriginProviderType provider)
{
  m_crosshairsOriginProvider = provider;
}

void CameraInteractionHandler::setRefSpaceAABBoxCenterProvider(
  RefSpaceAABBoxCenterProviderType provider
)
{
  m_refSpaceCenterProvider = provider;
}

void CameraInteractionHandler::setRefSpaceVoxelScaleProvider(RefSpaceVoxelScaleProviderType provider)
{
  m_refSpaceVoxelScaleProvider = provider;
}

void CameraInteractionHandler::setRefSpaceAABBoxSizeProvider(RefSpaceAABBoxSizeProviderType provider)
{
  m_refSpaceAABBoxSizeProvider = provider;
}

void CameraInteractionHandler::setZoomSynchronizer(ZoomSynchronizer synchronizer)
{
  m_zoomSynchronizer = synchronizer;
}

void CameraInteractionHandler::setWorldCameraPositionBroadcaster(SetterType<glm::vec3> broadcaster)
{
  m_worldCameraPositionBroadcaster = broadcaster;
}

void CameraInteractionHandler::setMode(const CameraInteractionMode& mode)
{
  m_primaryMode = mode;
  m_mouseMoveMode = msk_defaultInternalModeMap.at(m_primaryMode);
}

bool CameraInteractionHandler::
  doHandleMouseDoubleClickEvent(const QMouseEvent*, const Viewport&, const camera::Camera&)
{
  return false;
}

bool CameraInteractionHandler::
  doHandleMouseMoveEvent(const QMouseEvent* event, const Viewport& viewport, const camera::Camera&)
{
  if (MouseMoveMode::None == m_mouseMoveMode)
  {
    return false;
  }

  if (!m_crosshairsOriginProvider || !m_cameraProvider)
  {
    return false;
  }

  auto camera = m_cameraProvider();
  if (!camera)
  {
    return false;
  }

  bool handled = false;

  const glm::vec2 ndcPos = camera::ndc2d_O_mouse(viewport, {event->x(), event->y()});

  const bool controlModifier = (Qt::ControlModifier & event->modifiers());
  const bool shiftModifier = (Qt::ShiftModifier & event->modifiers());

  if (Qt::LeftButton & event->buttons())
  {
    switch (m_mouseMoveMode)
    {
    case MouseMoveMode::Translate:
    {
      if (controlModifier)
      {
        float scale = (shiftModifier) ? 100.0f : 50.0f;

        if (m_refSpaceVoxelScaleProvider)
        {
          scale *= m_refSpaceVoxelScaleProvider();
        }

        translateInOut(*camera, m_ndcLeftButtonLastPos, ndcPos, scale);
        handled = true;
      }
      else
      {
        if (!camera->isOrthographic())
        {
          const glm::vec3 worldPos = m_crosshairsOriginProvider();
          panRelativeToWorldPosition(*camera, m_ndcLeftButtonLastPos, ndcPos, worldPos);
          handled = true;
        }
        else if (m_refSpaceCenterProvider)
        {
          const glm::vec3 worldPos = m_refSpaceCenterProvider();
          panRelativeToWorldPosition(*camera, m_ndcLeftButtonLastPos, ndcPos, worldPos);
          handled = true;
        }
      }
      break;
    }
    case MouseMoveMode::RotateAboutImageCenter:
    {
      const glm::vec3 worldPos = m_refSpaceCenterProvider();
      rotateAboutWorldPoint(*camera, m_ndcLeftButtonLastPos, ndcPos, worldPos);
      handled = true;
      break;
    }
    case MouseMoveMode::RotateAboutCrosshairs:
    {
      rotateAboutWorldPoint(*camera, m_ndcLeftButtonLastPos, ndcPos, m_crosshairsOriginProvider());
      handled = true;
      break;
    }
    case MouseMoveMode::RotateInPlane:
    {
      const glm::vec2 ndcRotationCenter
        = (camera->isOrthographic()) ? glm::vec2{ndc_O_world(*camera, m_crosshairsOriginProvider())}
                                     : glm::vec2{0.0f, 0.0f};

      rotateInPlane(*camera, m_ndcLeftButtonLastPos, ndcPos, ndcRotationCenter);
      handled = true;
      break;
    }
    case MouseMoveMode::ZoomAboutPoint:
    {
      // Zoom in to crosshairs?
      zoomNdc(*camera, m_ndcLeftButtonLastPos, ndcPos, m_ndcLeftButtonStartPos);

      if (m_zoomSynchronizer)
      {
        const glm::vec3 worldCrosshairsOrigin = m_crosshairsOriginProvider();
        const glm::vec3 ndcCrosshairsOrigin = camera::ndc_O_world(*camera, worldCrosshairsOrigin);
        const glm::vec3 worldCenterPos
          = camera::world_O_ndc(*camera, glm::vec3{ndcPos, ndcCrosshairsOrigin.z});

        m_zoomSynchronizer(camera->getZoom(), worldCenterPos);
      }

      handled = true;
      break;
    }
    case MouseMoveMode::None:
    {
      break;
    }
    default:
      break;
    }

    m_ndcLeftButtonLastPos = ndcPos;
  }
  else if (Qt::RightButton & event->buttons())
  {
    switch (m_mouseMoveMode)
    {
    case MouseMoveMode::TranslateFrontBack:
    {
      float scale = (shiftModifier) ? 100.0f : 50.0f;

      if (m_refSpaceVoxelScaleProvider)
      {
        scale *= m_refSpaceVoxelScaleProvider();
      }

      translateInOut(*camera, m_ndcRightButtonLastPos, ndcPos, scale);
      handled = true;
      break;
    }
    case MouseMoveMode::RotateAboutCameraOrigin:
    {
      rotateAboutCameraOrigin(*camera, m_ndcRightButtonLastPos, ndcPos);
      handled = true;
      break;
    }
    case MouseMoveMode::ZoomAboutCenter:
    {
      static const glm::vec2 sk_ndcCenter{0.0f, 0.0f};
      zoomNdc(*camera, m_ndcRightButtonLastPos, ndcPos, sk_ndcCenter);

      if (m_zoomSynchronizer)
      {
        const glm::vec3 worldCrosshairsOrigin = m_crosshairsOriginProvider();
        const glm::vec3 ndcCrosshairsOrigin = camera::ndc_O_world(*camera, worldCrosshairsOrigin);
        const glm::vec3 worldCenterPos
          = camera::world_O_ndc(*camera, glm::vec3{ndcPos, ndcCrosshairsOrigin.z});

        m_zoomSynchronizer(camera->getZoom(), worldCenterPos);
      }

      handled = true;
      break;
    }
    case MouseMoveMode::None:
    {
      break;
    }
    default:
      break;
    }

    m_ndcRightButtonLastPos = ndcPos;
  }
  else if (Qt::MiddleButton & event->buttons())
  {
    m_ndcMiddleButtonLastPos = ndcPos;
  }

  if (handled && m_worldCameraPositionBroadcaster)
  {
    m_worldCameraPositionBroadcaster(worldOrigin(*camera));
  }

  return handled;
}

bool CameraInteractionHandler::doHandleMousePressEvent(
  const QMouseEvent* event, const Viewport& viewport, const camera::Camera& camera
)
{
  bool handled = false;

  const glm::vec2 ndcPos = camera::ndc2d_O_mouse(viewport, {event->x(), event->y()});

  const bool controlModifier = (Qt::ControlModifier & event->modifiers());
  const bool shiftModifier = (Qt::ShiftModifier & event->modifiers());

  if (Qt::LeftButton & event->button())
  {
    m_ndcLeftButtonStartPos = ndcPos;
    m_ndcLeftButtonLastPos = ndcPos;

    switch (m_primaryMode)
    {
    case CameraInteractionMode::Translate:
    {
      m_mouseMoveMode = MouseMoveMode::Translate;
      handled = true;
      break;
    }
    case CameraInteractionMode::Rotate:
    {
      if (controlModifier)
      {
        m_mouseMoveMode = MouseMoveMode::RotateAboutImageCenter;
      }
      else if (shiftModifier)
      {
        m_mouseMoveMode = MouseMoveMode::RotateInPlane;
      }
      else
      {
        m_mouseMoveMode = MouseMoveMode::RotateAboutCrosshairs;
      }
      handled = true;
      break;
    }
    case CameraInteractionMode::Zoom:
    {
      m_mouseMoveMode = MouseMoveMode::ZoomAboutPoint;
      handled = true;
      break;
    }
    }
  }
  else if (Qt::RightButton & event->button())
  {
    m_ndcRightButtonStartPos = ndcPos;
    m_ndcRightButtonLastPos = ndcPos;

    switch (m_primaryMode)
    {
    case CameraInteractionMode::Translate:
    {
      m_mouseMoveMode = MouseMoveMode::TranslateFrontBack;
      handled = true;
      break;
    }
    case CameraInteractionMode::Rotate:
    {
      if (!camera.isOrthographic())
      {
        m_mouseMoveMode = MouseMoveMode::RotateAboutCameraOrigin;
      }

      handled = true;
      break;
    }
    case CameraInteractionMode::Zoom:
    {
      m_mouseMoveMode = MouseMoveMode::ZoomAboutCenter;
      handled = true;
      break;
    }
    }
  }
  else if (Qt::MiddleButton & event->button())
  {
    m_ndcMiddleButtonStartPos = ndcPos;
    m_ndcMiddleButtonLastPos = ndcPos;

    switch (m_primaryMode)
    {
    case CameraInteractionMode::Translate:
      break;
    case CameraInteractionMode::Rotate:
      break;
    case CameraInteractionMode::Zoom:
      break;
    }
  }

  return handled;
}

bool CameraInteractionHandler::
  doHandleMouseReleaseEvent(const QMouseEvent* event, const Viewport& viewport, const camera::Camera&)
{
  bool handled = false;

  const glm::vec2 ndcPos = camera::ndc2d_O_mouse(viewport, {event->x(), event->y()});

  if (Qt::LeftButton & event->button())
  {
    m_ndcLeftButtonLastPos = ndcPos;
    m_mouseMoveMode = MouseMoveMode::None;
    handled = true;
  }
  else if (Qt::RightButton & event->button())
  {
    m_ndcRightButtonLastPos = ndcPos;
    m_mouseMoveMode = MouseMoveMode::None;
    handled = true;
  }
  else if (Qt::MiddleButton & event->button())
  {
    m_ndcMiddleButtonLastPos = ndcPos;
    handled = true;
  }

  return handled;
}

bool CameraInteractionHandler::
  doHandleWheelEvent(const QWheelEvent* event, const Viewport& viewport, const camera::Camera&)
{
  if (!m_crosshairsOriginProvider || !m_cameraProvider)
  {
    return false;
  }

  auto camera = m_cameraProvider();
  if (!camera)
  {
    return false;
  }

  bool handled = false;

  //    event->orientation(); Qt::Orientation;
  //    event->phase(); Qt::ScrollPhase;

  const bool shiftModifier = (Qt::ShiftModifier & event->modifiers());

  const float k_inv = (event->inverted()) ? -1.0f : 1.0f;
  const float k_numDegrees = k_inv * event->angleDelta().y() / 8.0f;

  switch (m_primaryMode)
  {
  case CameraInteractionMode::Translate: // [[fallthrough]];
                                         /// @todo Make it pan?
  case CameraInteractionMode::Rotate:    // [[fallthrough]];
  case CameraInteractionMode::Zoom:
  {
    if (camera->isOrthographic())
    {
      const float k_numClicks = k_numDegrees / 45.0f;
      const glm::vec2 ndcPos = camera::ndc2d_O_mouse(viewport, {event->x(), event->y()});
      zoomNdcDelta(*camera, k_numClicks, ndcPos);

      if (m_zoomSynchronizer)
      {
        const glm::vec3 worldCrosshairsOrigin = m_crosshairsOriginProvider();
        const glm::vec3 ndcCrosshairsOrigin = camera::ndc_O_world(*camera, worldCrosshairsOrigin);
        const glm::vec3 worldCenterPos
          = camera::world_O_ndc(*camera, glm::vec3{ndcPos, ndcCrosshairsOrigin.z});

        m_zoomSynchronizer(camera->getZoom(), worldCenterPos);
      }

      handled = true;

      // Could also zoom inwards towards pointer position:
      //            const glm::vec2 ndcPos =
      //                    ( numDegrees > 0.0f && m_crosshairsOriginProvider )
      //                    ? ndc_O_world( *camera, m_crosshairsOriginProvider() )
      //                    : glm::vec2{ 0.0f, 0.0f };
    }
    else
    {
      // Move the camera position itself for perspective views:
      const glm::vec2 ndcPos = camera::ndc2d_O_mouse(viewport, {event->x(), event->y()});
      const glm::vec3 cameraVec = cameraRayDirection(*camera, ndcPos);

      float scale = (shiftModifier) ? 2.0f : 0.5f;

      if (m_refSpaceVoxelScaleProvider)
      {
        scale *= m_refSpaceVoxelScaleProvider();
      }

      translateAboutCamera(*camera, scale * k_numDegrees * cameraVec);

      // Adjust focal distance:
      /// @todo Take this logic out of here and set it with broadcaster callback.
      /// @todo This adjustment should be made when ever the camera is moved, not only
      /// in this location.

      // Position of crosshairs origin in Camera space
      const glm::vec4 cameraCrosshairsPos = camera->camera_O_world()
                                            * glm::vec4{m_crosshairsOriginProvider(), 1.0f};

      // Signed distance from camera origin to crosshairs origin
      const float distance = cameraCrosshairsPos.z / cameraCrosshairsPos.w;

      if (m_refSpaceAABBoxSizeProvider && distance < 0.0f && std::abs(distance) > camera->nearDistance())
      {
        // Crosshairs origin is in front of camera and beyond the near distance

        // Extra amount to add to far distance to encompass scene
        const float sceneSize = glm::length(m_refSpaceAABBoxSizeProvider());

        // Don't extend far distance beyond 10x the scene size
        const float maxFarDistance = 10.0f * sceneSize;

        camera->setFarDistance(std::min(2.0f * std::abs(distance) + sceneSize, maxFarDistance));
      }

      handled = true;
    }
    break;
  }
  }

  if (handled && m_worldCameraPositionBroadcaster)
  {
    m_worldCameraPositionBroadcaster(worldOrigin(*camera));
  }

  return handled;
}

bool CameraInteractionHandler::
  doHandlePanGesture(const QPanGesture*, const Viewport&, const camera::Camera&)
{
  bool handled = false;
  return handled;
}

bool CameraInteractionHandler::
  doHandlePinchGesture(const QPinchGesture* gesture, const Viewport& viewport, const camera::Camera&)
{
  if (!m_cameraProvider)
  {
    return false;
  }

  auto camera = m_cameraProvider();
  if (!camera)
  {
    return false;
  }

  bool handled = false;

  const glm::vec2 centerPoint(gesture->centerPoint().x(), gesture->centerPoint().y());

  const glm::vec2 ndcPos = (camera->isOrthographic()) ? camera::ndc2d_O_mouse(viewport, centerPoint)
                                                      : glm::vec2{0.0f, 0.0f};

  const QPinchGesture::ChangeFlags changeFlags = gesture->changeFlags();

  if (QPinchGesture::RotationAngleChanged & changeFlags)
  {
    const double angle = -glm::radians(gesture->rotationAngle() - gesture->lastRotationAngle());
    rotateInPlane(*camera, static_cast<float>(angle), ndcPos);
    handled = true;
  }

  if (QPinchGesture::ScaleFactorChanged & changeFlags)
  {
    zoomNdc(*camera, static_cast<float>(gesture->scaleFactor()), ndcPos);

    if (m_zoomSynchronizer && m_crosshairsOriginProvider)
    {
      const glm::vec3 worldCrosshairsOrigin = m_crosshairsOriginProvider();
      const glm::vec3 ndcCrosshairsOrigin = camera::ndc_O_world(*camera, worldCrosshairsOrigin);
      const glm::vec3 worldCenterPos
        = camera::world_O_ndc(*camera, glm::vec3{ndcPos, ndcCrosshairsOrigin.z});

      m_zoomSynchronizer(camera->getZoom(), worldCenterPos);
    }

    handled = true;
  }

  if (QPinchGesture::CenterPointChanged & changeFlags)
  {
    // Not used
  }

  if (Qt::GestureFinished == gesture->state())
  {
    // Not used
  }

  return handled;
}

bool CameraInteractionHandler::
  doHandleSwipeGesture(const QSwipeGesture* gesture, const Viewport&, const camera::Camera&)
{
  bool handled = false;

  if (Qt::GestureFinished == gesture->state())
  {
    if (QSwipeGesture::Left == gesture->horizontalDirection() || QSwipeGesture::Up == gesture->verticalDirection())
    {
      /// @note Not used
    }
    else
    {
      /// @note Not used
    }
  }

  return handled;
}
