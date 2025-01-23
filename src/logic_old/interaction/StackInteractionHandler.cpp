#include "logic/interaction/StackInteractionHandler.h"
#include "common/CoordinateFrame.h"
#include "logic/camera/CameraHelpers.h"

SlideStackInteractionHandler::SlideStackInteractionHandler()
  : InteractionHandlerBase(InteractionHandlerType::StackTransform)
  ,

  m_stackFrameProvider(nullptr)
  , m_stackFrameChangedBroadcaster(nullptr)
  , m_stackFrameDoneBroadcaster(nullptr)
  , m_activeImageVoxelScaleProvider(nullptr)
  ,

  m_primaryMode(StackInteractionMode::Translate)
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
  // m_stackFrameChangedBroadcaster and m_stackFrameDoneBroadcaster
  setUpdatesViewsOnEventHandled(false);
}

void SlideStackInteractionHandler::setSlideStackFrameProvider(GetterType<CoordinateFrame> responder)
{
  m_stackFrameProvider = responder;
}

void SlideStackInteractionHandler::setSlideStackFrameChangedBroadcaster(
  SetterType<const CoordinateFrame&> broadcaster
)
{
  m_stackFrameChangedBroadcaster = broadcaster;
}

void SlideStackInteractionHandler::setSlideStackFrameChangeDoneBroadcaster(
  SetterType<const CoordinateFrame&> broadcaster
)
{
  m_stackFrameDoneBroadcaster = broadcaster;
}

void SlideStackInteractionHandler::setRefImageVoxelScaleProvider(GetterType<float> responder)
{
  m_activeImageVoxelScaleProvider = responder;
}

void SlideStackInteractionHandler::setMode(const StackInteractionMode& mode)
{
  m_primaryMode = mode;
  m_mouseMoveMode = MouseMoveMode::None;
}

bool SlideStackInteractionHandler::
  doHandleMouseDoubleClickEvent(const QMouseEvent*, const Viewport&, const camera::Camera&)
{
  return false;
}

bool SlideStackInteractionHandler::doHandleMouseMoveEvent(
  const QMouseEvent* event, const Viewport& viewport, const camera::Camera& camera
)
{
  bool handled = false;

  if (MouseMoveMode::None == m_mouseMoveMode)
  {
    return handled;
  }

  if (!m_stackFrameProvider || !m_stackFrameChangedBroadcaster)
  {
    return handled;
  }

  auto stackFrame = m_stackFrameProvider();

  const glm::vec2 ndcPos = camera::ndc2d_O_mouse(viewport, {event->x(), event->y()});

  const bool shiftModifier = (Qt::ShiftModifier & event->modifiers());

  if (Qt::LeftButton & event->buttons())
  {
    switch (m_mouseMoveMode)
    {
    case MouseMoveMode::TranslateInPlane:
    {
      const float ndcZ = ndcZofWorldPoint(camera, stackFrame.worldOrigin());
      const glm::vec3 T = translationInCameraPlane(camera, m_ndcLeftButtonLastPos, ndcPos, ndcZ);

      stackFrame.setWorldOrigin(stackFrame.worldOrigin() + T);
      handled = true;

      break;
    }
    case MouseMoveMode::TranslateFrontBack:
    {
      float scale = (shiftModifier) ? 100.0f : 50.0f;

      if (m_activeImageVoxelScaleProvider)
      {
        scale *= m_activeImageVoxelScaleProvider();
      }

      const glm::vec3 T
        = translationAboutCameraFrontBack(camera, m_ndcLeftButtonLastPos, ndcPos, scale);

      stackFrame.setWorldOrigin(stackFrame.worldOrigin() + T);
      handled = true;

      break;
    }
    case MouseMoveMode::Rotate2DInPlane:
    {
      const glm::vec2 ndcRotationCenter = ndc_O_world(camera, stackFrame.worldOrigin());

      const glm::quat R
        = rotation2dInCameraPlane(camera, m_ndcLeftButtonLastPos, ndcPos, ndcRotationCenter);

      stackFrame.setFrameToWorldRotation(R * stackFrame.world_O_frame_rotation());
      handled = true;

      break;
    }
    case MouseMoveMode::Rotate3DAboutPlane:
    {
      const glm::quat R = rotation3dAboutCameraPlane(camera, m_ndcLeftButtonLastPos, ndcPos);
      stackFrame.setFrameToWorldRotation(R * stackFrame.world_O_frame_rotation());
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
    m_stackFrameChangedBroadcaster(stackFrame);
  }

  return handled;
}

bool SlideStackInteractionHandler::
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
    case StackInteractionMode::Translate:
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
    case StackInteractionMode::Rotate:
    {
      if (controlModifier)
      {
        m_mouseMoveMode = MouseMoveMode::Rotate3DAboutPlane;
      }
      else
      {
        m_mouseMoveMode = MouseMoveMode::Rotate2DInPlane;
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
    case StackInteractionMode::Translate:
    {
      m_mouseMoveMode = MouseMoveMode::None;
      handled = true;
      break;
    }
    case StackInteractionMode::Rotate:
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
    case StackInteractionMode::Translate:
    {
      m_mouseMoveMode = MouseMoveMode::None;
      handled = true;
      break;
    }
    case StackInteractionMode::Rotate:
    {
      m_mouseMoveMode = MouseMoveMode::None;
      handled = true;
      break;
    }
    }
  }

  return handled;
}

bool SlideStackInteractionHandler::
  doHandleMouseReleaseEvent(const QMouseEvent* event, const Viewport& viewport, const camera::Camera&)
{
  bool handled = false;

  if (!m_stackFrameProvider || !m_stackFrameDoneBroadcaster)
  {
    return handled;
  }

  const glm::vec2 ndcPos = camera::ndc2d_O_mouse(viewport, {event->x(), event->y()});

  if (Qt::LeftButton & event->button())
  {
    m_ndcLeftButtonLastPos = ndcPos;

    switch (m_primaryMode)
    {
    case StackInteractionMode::Translate:
    {
      m_mouseMoveMode = MouseMoveMode::None;
      handled = true;
      break;
    }
    case StackInteractionMode::Rotate:
    {
      m_mouseMoveMode = MouseMoveMode::None;
      handled = true;
      break;
    }
    }

    m_stackFrameDoneBroadcaster(m_stackFrameProvider());
  }
  else if (Qt::RightButton & event->button())
  {
    m_ndcRightButtonLastPos = ndcPos;

    switch (m_primaryMode)
    {
    case StackInteractionMode::Translate:
    {
      m_mouseMoveMode = MouseMoveMode::None;
      handled = true;
      break;
    }
    case StackInteractionMode::Rotate:
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
    case StackInteractionMode::Translate:
    {
      m_mouseMoveMode = MouseMoveMode::None;
      handled = true;
      break;
    }
    case StackInteractionMode::Rotate:
    {
      m_mouseMoveMode = MouseMoveMode::None;
      handled = true;
      break;
    }
    }
  }

  return handled;
}
