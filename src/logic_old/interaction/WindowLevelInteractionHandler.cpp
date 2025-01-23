#include "logic/interaction/WindowLevelInteractionHandler.h"
#include "logic/camera/CameraHelpers.h"

#include "imageio/ImageCpuRecord.h"

#include <glm/glm.hpp>

namespace
{

static constexpr uint sk_comp = 0;

}

WindowLevelInteractionHandler::WindowLevelInteractionHandler()
  : InteractionHandlerBase(InteractionHandlerType::WindowLevel)
  ,

  m_activeImageRequester(nullptr)
  , m_activeImageWindowLevelBroadcaster(nullptr)
  ,

  m_primaryMode(WindowLevelInteractionMode::Default)
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
  // m_activeImageWindowLevelBroadcaster
  setUpdatesViewsOnEventHandled(false);
}

void WindowLevelInteractionHandler::setActiveImageCpuRecordRequester(
  ActiveImageCpuRecordRequesterType provider
)
{
  m_activeImageRequester = provider;
}

void WindowLevelInteractionHandler::setActiveImageWindowLevelBroadcaster(
  ActiveImageWindowLevelBroadcasterType broadcaster
)
{
  m_activeImageWindowLevelBroadcaster = broadcaster;
}

void WindowLevelInteractionHandler::setMode(const WindowLevelInteractionMode& mode)
{
  m_primaryMode = mode;
  m_mouseMoveMode = MouseMoveMode::None;
}

bool WindowLevelInteractionHandler::
  doHandleMouseDoubleClickEvent(const QMouseEvent*, const Viewport&, const camera::Camera&)
{
  return false;
}

bool WindowLevelInteractionHandler::
  doHandleMouseMoveEvent(const QMouseEvent* event, const Viewport& viewport, const camera::Camera&)
{
  bool handled = false;

  if (MouseMoveMode::None == m_mouseMoveMode)
  {
    return handled;
  }

  const glm::vec2 ndcPos = camera::ndc2d_O_mouse(viewport, {event->x(), event->y()});

  if (Qt::LeftButton & event->buttons())
  {
    switch (m_mouseMoveMode)
    {
    case MouseMoveMode::WindowAndLevel:
    {
      const double scaleFactor = (Qt::ShiftModifier & event->modifiers()) ? 1.00 : 0.25;
      handled = changeWindowLevel(m_ndcLeftButtonLastPos, ndcPos, scaleFactor);
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
    switch (m_mouseMoveMode)
    {
    case MouseMoveMode::WindowAndLevel:
    {
      const double scaleFactor = (Qt::ShiftModifier & event->modifiers()) ? 1.00 : 0.25;
      handled = changeWindowLevel(m_ndcRightButtonLastPos, ndcPos, scaleFactor);
      break;
    }
    case MouseMoveMode::None:
    {
      break;
    }
    }

    m_ndcRightButtonLastPos = ndcPos;
  }
  else if (Qt::MiddleButton & event->buttons())
  {
    switch (m_mouseMoveMode)
    {
    case MouseMoveMode::WindowAndLevel:
    {
      const double scaleFactor = (Qt::ShiftModifier & event->modifiers()) ? 1.00 : 0.25;
      handled = changeWindowLevel(m_ndcMiddleButtonLastPos, ndcPos, scaleFactor);
      break;
    }
    case MouseMoveMode::None:
    {
      break;
    }
    }

    m_ndcMiddleButtonLastPos = ndcPos;
  }

  return handled;
}

bool WindowLevelInteractionHandler::
  doHandleMousePressEvent(const QMouseEvent* event, const Viewport& viewport, const camera::Camera&)
{
  bool handled = false;

  const glm::vec2 ndcPos = camera::ndc2d_O_mouse(viewport, {event->x(), event->y()});

  if (Qt::LeftButton & event->button())
  {
    m_ndcLeftButtonStartPos = ndcPos;
    m_ndcLeftButtonLastPos = ndcPos;

    switch (m_primaryMode)
    {
    case WindowLevelInteractionMode::Default:
    {
      m_mouseMoveMode = MouseMoveMode::WindowAndLevel;
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
    case WindowLevelInteractionMode::Default:
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
    case WindowLevelInteractionMode::Default:
    {
      m_mouseMoveMode = MouseMoveMode::None;
      handled = true;
      break;
    }
    }
  }

  return handled;
}

bool WindowLevelInteractionHandler::
  doHandleMouseReleaseEvent(const QMouseEvent* event, const Viewport& viewport, const camera::Camera&)
{
  bool handled = false;

  const glm::vec2 ndcPos = camera::ndc2d_O_mouse(viewport, {event->x(), event->y()});

  if (Qt::LeftButton & event->button())
  {
    m_ndcLeftButtonLastPos = ndcPos;

    switch (m_primaryMode)
    {
    case WindowLevelInteractionMode::Default:
    {
      m_mouseMoveMode = MouseMoveMode::None;
      handled = true;
      break;
    }
    }
  }
  else if (Qt::RightButton & event->button())
  {
    m_ndcRightButtonLastPos = ndcPos;
    handled = true;
  }
  else if (Qt::MiddleButton & event->button())
  {
    m_ndcMiddleButtonLastPos = ndcPos;
    handled = true;
  }

  return handled;
}

bool WindowLevelInteractionHandler::
  doHandleWheelEvent(const QWheelEvent* event, const Viewport&, const camera::Camera&)
{
  bool handled = false;

  //    event->phase(); Qt::ScrollPhase;

  const double inv = (event->inverted()) ? -1.0 : 1.0;
  const double numDegrees = event->angleDelta().y() / 8.0;
  const double delta = inv * numDegrees / 45.0;

  switch (m_primaryMode)
  {
  case WindowLevelInteractionMode::Default:
  {
    const double scaleFactor = (Qt::ShiftModifier & event->modifiers()) ? 1.00 : 0.25;

    if (Qt::Orientation::Horizontal == event->orientation())
    {
      handled = changeLevel(delta, scaleFactor);
    }
    else if (Qt::Orientation::Vertical == event->orientation())
    {
      handled = changeWindow(delta, scaleFactor);
    }

    break;
  }
  }

  return handled;
}

bool WindowLevelInteractionHandler::changeWindowLevel(
  const glm::vec2& ndcOldPos, const glm::vec2& ndcNewPos, double scaleFactor
)
{
  bool handled = changeWindow(ndcOldPos, ndcNewPos, scaleFactor);
  handled |= changeLevel(ndcOldPos, ndcNewPos, scaleFactor);
  return handled;
}

bool WindowLevelInteractionHandler::changeWindow(
  const glm::vec2& ndcOldPos, const glm::vec2& ndcNewPos, double scaleFactor
)
{
  return changeWindow(static_cast<double>(ndcNewPos.y - ndcOldPos.y), scaleFactor);
}

bool WindowLevelInteractionHandler::changeLevel(
  const glm::vec2& ndcOldPos, const glm::vec2& ndcNewPos, double scaleFactor
)
{
  return changeLevel(static_cast<double>(ndcNewPos.x - ndcOldPos.x), scaleFactor);
}

bool WindowLevelInteractionHandler::changeWindow(double delta, double scaleFactor)
{
  bool handled = false;

  if (!m_activeImageRequester || !m_activeImageWindowLevelBroadcaster)
  {
    return handled;
  }

  if (const auto* cpuRecord = m_activeImageRequester())
  {
    const auto windowRange = cpuRecord->settings().windowRange(sk_comp);
    const double F = scaleFactor * (windowRange.second - windowRange.first);
    const double window = cpuRecord->settings().window(sk_comp);
    const double level = cpuRecord->settings().level(sk_comp);

    m_activeImageWindowLevelBroadcaster(window + F * delta, level);
    handled = true;
  }

  return handled;
}

bool WindowLevelInteractionHandler::changeLevel(double delta, double scaleFactor)
{
  bool handled = false;

  if (!m_activeImageRequester || !m_activeImageWindowLevelBroadcaster)
  {
    return handled;
  }

  if (auto cpuRecord = m_activeImageRequester())
  {
    const auto levelRange = cpuRecord->settings().levelRange(sk_comp);
    const double F = scaleFactor * (levelRange.second - levelRange.first);
    const double window = cpuRecord->settings().window(sk_comp);
    const double level = cpuRecord->settings().level(sk_comp);

    m_activeImageWindowLevelBroadcaster(window, level + F * delta);
    handled = true;
  }

  return handled;
}
