#include "logic/interaction/RefImageInteractionHandler.h"
#include "common/CoordinateFrame.h"
#include "logic/camera/CameraHelpers.h"

namespace
{

/**
 * @brief Apply rotation to a coordinate frame about a given world center position
 * @param frame Frame to rotate
 * @param rotation Rotation, expressed as a quaternion
 * @param worldCenter Center of rotation in World space
 */
void rotateFrameAboutWorldPos(
  CoordinateFrame& frame, const glm::quat& rotation, const glm::vec3& worldCenter
)
{
  const glm::quat oldRotation = frame.world_O_frame_rotation();
  const glm::vec3 oldOrigin = frame.worldOrigin();

  frame.setFrameToWorldRotation(rotation * oldRotation);
  frame.setWorldOrigin(rotation * (oldOrigin - worldCenter) + worldCenter);
}

} // namespace

RefImageInteractionHandler::RefImageInteractionHandler()
  : InteractionHandlerBase(InteractionHandlerType::StackTransform)
  ,

  m_crosshairsOriginProvider(nullptr)
  , m_imageFrameProvider(nullptr)
  , m_imageFrameChangedBroadcaster(nullptr)
  , m_imageFrameDoneBroadcaster(nullptr)
  , m_imageVoxelScaleProvider(nullptr)
  ,

  m_primaryMode(RefImageInteractionMode::Translate)
  , m_mouseMoveMode(MouseMoveMode::None)
  ,

  m_ndcLeftButtonStartPos(0.0f)
  , m_ndcRightButtonStartPos(0.0f)
  , m_ndcMiddleButtonStartPos(0.0f)
  , m_ndcLeftButtonLastPos(0.0f)
  , m_ndcRightButtonLastPos(0.0f)
  , m_ndcMiddleButtonLastPos(0.0f)
{
  // Do not update views when this class handles events. Instead, updates will be handled by
  // m_imageFrameChangedBroadcaster and m_imageFrameDoneBroadcaster
  setUpdatesViewsOnEventHandled(false);
}

void RefImageInteractionHandler::setCrosshairsOriginProvider(GetterType<glm::vec3> provider)
{
  m_crosshairsOriginProvider = provider;
}

void RefImageInteractionHandler::setImageFrameProvider(
  GetterType<std::optional<CoordinateFrame> > provider
)
{
  m_imageFrameProvider = provider;
}

void RefImageInteractionHandler::setImageFrameChangedBroadcaster(
  SetterType<const CoordinateFrame&> broadcaster
)
{
  m_imageFrameChangedBroadcaster = broadcaster;
}

void RefImageInteractionHandler::setImageFrameChangeDoneBroadcaster(
  SetterType<const CoordinateFrame&> broadcaster
)
{
  m_imageFrameDoneBroadcaster = broadcaster;
}

void RefImageInteractionHandler::setImageVoxelScaleProvider(GetterType<float> responder)
{
  m_imageVoxelScaleProvider = responder;
}

void RefImageInteractionHandler::setMode(const RefImageInteractionMode& mode)
{
  m_primaryMode = mode;
  m_mouseMoveMode = MouseMoveMode::None;
}

bool RefImageInteractionHandler::
  doHandleMouseDoubleClickEvent(const QMouseEvent*, const Viewport&, const camera::Camera&)
{
  return false;
}

bool RefImageInteractionHandler::doHandleMouseMoveEvent(
  const QMouseEvent* event, const Viewport& viewport, const camera::Camera& camera
)
{
  if (MouseMoveMode::None == m_mouseMoveMode)
  {
    return false;
  }

  if (!m_crosshairsOriginProvider || !m_imageFrameProvider || !m_imageFrameChangedBroadcaster)
  {
    return false;
  }

  auto imageFrame = m_imageFrameProvider();
  if (!imageFrame)
  {
    return false;
  }

  bool handled = false;

  const glm::vec2 ndcPos = camera::ndc2d_O_mouse(viewport, {event->x(), event->y()});

  const bool shiftModifier = (Qt::ShiftModifier & event->modifiers());

  if (Qt::LeftButton & event->buttons())
  {
    switch (m_mouseMoveMode)
    {
    case MouseMoveMode::TranslateInPlane:
    {
      const float ndcZ = ndcZofWorldPoint(camera, imageFrame->worldOrigin());
      const glm::vec3 T = translationInCameraPlane(camera, m_ndcLeftButtonLastPos, ndcPos, ndcZ);

      imageFrame->setWorldOrigin(imageFrame->worldOrigin() + T);
      handled = true;
      break;
    }
    case MouseMoveMode::TranslateFrontBack:
    {
      float scale = (shiftModifier) ? 100.0f : 50.0f;

      if (m_imageVoxelScaleProvider)
      {
        scale *= m_imageVoxelScaleProvider();
      }

      const glm::vec3 T
        = translationAboutCameraFrontBack(camera, m_ndcLeftButtonLastPos, ndcPos, scale);

      imageFrame->setWorldOrigin(imageFrame->worldOrigin() + T);
      handled = true;
      break;
    }
    case MouseMoveMode::Rotate2dInPlane:
    {
      // Center of rotation is the crosshairs origin:
      const glm::vec3 crosshairsWorldOrigin = m_crosshairsOriginProvider();
      const glm::vec2 ndcRotationCenter = ndc_O_world(camera, crosshairsWorldOrigin);
      const glm::quat R
        = rotation2dInCameraPlane(camera, m_ndcLeftButtonLastPos, ndcPos, ndcRotationCenter);

      rotateFrameAboutWorldPos(*imageFrame, R, crosshairsWorldOrigin);
      handled = true;
      break;
    }
    case MouseMoveMode::Rotate3dAboutPlane:
    {
      // Center of rotation is the crosshairs origin:
      const glm::vec3 crosshairsWorldOrigin = m_crosshairsOriginProvider();
      const glm::quat R = rotation3dAboutCameraPlane(camera, m_ndcLeftButtonLastPos, ndcPos);

      rotateFrameAboutWorldPos(*imageFrame, R, crosshairsWorldOrigin);
      handled = true;
      break;
    }
    case MouseMoveMode::None:
    {
      break;
    }
    }

    m_ndcLeftButtonLastPos = ndcPos;
  }
  else if (Qt::RightButton & event->buttons())
  {
    m_ndcRightButtonLastPos = ndcPos;
  }
  else if (Qt::MiddleButton & event->buttons())
  {
    m_ndcMiddleButtonLastPos = ndcPos;
  }

  if (handled)
  {
    m_imageFrameChangedBroadcaster(*imageFrame);
  }

  return handled;
}

bool RefImageInteractionHandler::
  doHandleMousePressEvent(const QMouseEvent* event, const Viewport& viewport, const camera::Camera&)
{
  bool handled = false;

  const glm::vec2 ndcPos = camera::ndc2d_O_mouse(viewport, {event->x(), event->y()});

  //    const bool shiftModifier = ( Qt::ShiftModifier & event->modifiers() );
  const bool controlModifier = (Qt::ControlModifier & event->modifiers());

  if (Qt::LeftButton & event->button())
  {
    m_ndcLeftButtonStartPos = ndcPos;
    m_ndcLeftButtonLastPos = ndcPos;

    switch (m_primaryMode)
    {
    case RefImageInteractionMode::Translate:
    {
      if (controlModifier)
      {
        m_mouseMoveMode = MouseMoveMode::TranslateFrontBack;
      }
      else
      {
        m_mouseMoveMode = MouseMoveMode::TranslateInPlane;
      }
      handled = true;
      break;
    }
    case RefImageInteractionMode::Rotate:
    {
      if (controlModifier)
      {
        m_mouseMoveMode = MouseMoveMode::Rotate3dAboutPlane;
      }
      else
      {
        m_mouseMoveMode = MouseMoveMode::Rotate2dInPlane;
      }
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
    case RefImageInteractionMode::Translate:
    {
      m_mouseMoveMode = MouseMoveMode::None;
      handled = true;
      break;
    }
    case RefImageInteractionMode::Rotate:
    {
      m_mouseMoveMode = MouseMoveMode::None;
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
    case RefImageInteractionMode::Translate:
    {
      m_mouseMoveMode = MouseMoveMode::None;
      handled = true;
      break;
    }
    case RefImageInteractionMode::Rotate:
    {
      m_mouseMoveMode = MouseMoveMode::None;
      handled = true;
      break;
    }
    }
  }

  return handled;
}

bool RefImageInteractionHandler::
  doHandleMouseReleaseEvent(const QMouseEvent* event, const Viewport& viewport, const camera::Camera&)
{
  bool handled = false;

  if (!m_imageFrameProvider || !m_imageFrameDoneBroadcaster)
  {
    return handled;
  }

  const glm::vec2 ndcPos = camera::ndc2d_O_mouse(viewport, {event->x(), event->y()});

  if (Qt::LeftButton & event->button())
  {
    m_ndcLeftButtonLastPos = ndcPos;

    switch (m_primaryMode)
    {
    case RefImageInteractionMode::Translate:
    {
      m_mouseMoveMode = MouseMoveMode::None;
      handled = true;
      break;
    }
    case RefImageInteractionMode::Rotate:
    {
      m_mouseMoveMode = MouseMoveMode::None;
      handled = true;
      break;
    }
    }

    if (auto frame = m_imageFrameProvider())
    {
      m_imageFrameDoneBroadcaster(*frame);
    }
  }
  else if (Qt::RightButton & event->button())
  {
    m_ndcRightButtonLastPos = ndcPos;

    switch (m_primaryMode)
    {
    case RefImageInteractionMode::Translate:
    {
      m_mouseMoveMode = MouseMoveMode::None;
      handled = true;
      break;
    }
    case RefImageInteractionMode::Rotate:
    {
      m_mouseMoveMode = MouseMoveMode::None;
      handled = true;
      break;
    }
    }
  }
  else if (Qt::MiddleButton & event->button())
  {
    m_ndcMiddleButtonLastPos = ndcPos;

    switch (m_primaryMode)
    {
    case RefImageInteractionMode::Translate:
    {
      m_mouseMoveMode = MouseMoveMode::None;
      handled = true;
      break;
    }
    case RefImageInteractionMode::Rotate:
    {
      m_mouseMoveMode = MouseMoveMode::None;
      handled = true;
      break;
    }
    }
  }

  return handled;
}
