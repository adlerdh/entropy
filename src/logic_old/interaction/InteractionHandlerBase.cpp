#include "logic/interaction/InteractionHandlerBase.h"

#include <QGestureEvent>
#include <QPanGesture>
#include <QPinchGesture>
#include <QSwipeGesture>

InteractionHandlerBase::InteractionHandlerBase(const InteractionHandlerType& type)
  : m_type(type)
  , m_allViewsUpdater(nullptr)
  , m_myViewUpdater(nullptr)
  , m_updatesViewsOnEventHandled(true)
{
}

const InteractionHandlerType& InteractionHandlerBase::type() const
{
  return m_type;
}

void InteractionHandlerBase::setAllViewsUpdater(AllViewsUpdaterType updater)
{
  m_allViewsUpdater = updater;
}

void InteractionHandlerBase::setMyViewUpdater(MyViewUpdater updater)
{
  m_myViewUpdater = updater;
}

bool InteractionHandlerBase::handleMouseDoubleClickEvent(
  QMouseEvent* event, const Viewport& viewport, const camera::Camera& camera
)
{
  if (!event /*|| ! (Qt::MouseEventNotSynthesized & event->source())*/)
  {
    return false;
  }

  bool handled = doHandleMouseMoveEvent(event, viewport, camera);
  viewUpdater(handled);

  event->setAccepted(handled);

  return handled;
}

bool InteractionHandlerBase::handleMouseMoveEvent(
  QMouseEvent* event, const Viewport& viewport, const camera::Camera& camera
)
{
  if (!event /*|| ! (Qt::MouseEventNotSynthesized & event->source())*/)
  {
    return false;
  }

  bool handled = doHandleMouseMoveEvent(event, viewport, camera);
  viewUpdater(handled);

  event->setAccepted(handled);

  return handled;
}

bool InteractionHandlerBase::handleMousePressEvent(
  QMouseEvent* event, const Viewport& viewport, const camera::Camera& camera
)
{
  if (!event /*|| ! (Qt::MouseEventNotSynthesized & event->source())*/)
  {
    return false;
  }

  bool handled = doHandleMousePressEvent(event, viewport, camera);
  viewUpdater(handled);

  event->setAccepted(handled);

  return handled;
}

bool InteractionHandlerBase::handleMouseReleaseEvent(
  QMouseEvent* event, const Viewport& viewport, const camera::Camera& camera
)
{
  if (!event /*|| ! (Qt::MouseEventNotSynthesized & event->source())*/)
  {
    return false;
  }

  bool handled = doHandleMouseReleaseEvent(event, viewport, camera);
  viewUpdater(handled);

  event->setAccepted(handled);

  return handled;
}

bool InteractionHandlerBase::handleTabletEvent(
  QTabletEvent* event, const Viewport& viewport, const camera::Camera& camera
)
{
  if (!event /*|| ! (Qt::MouseEventNotSynthesized & event->source())*/)
  {
    return false;
  }

  bool handled = doHandleTabletEvent(event, viewport, camera);
  viewUpdater(handled);

  event->setAccepted(handled);

  return handled;
}

bool InteractionHandlerBase::handleWheelEvent(
  QWheelEvent* event, const Viewport& viewport, const camera::Camera& camera
)
{
  if (!event)
  {
    return false;
  }

  bool handled = doHandleWheelEvent(event, viewport, camera);
  viewUpdater(handled);

  event->setAccepted(handled);

  return handled;
}

bool InteractionHandlerBase::dispatchGestureEvent(
  QGestureEvent* event, const Viewport& viewport, const camera::Camera& camera
)
{
  if (!event)
  {
    return false;
  }

  bool handled = false;

  if (QGesture* swipe = event->gesture(Qt::SwipeGesture))
  {
    if ((handled |= handleSwipeGesture(dynamic_cast<QSwipeGesture*>(swipe), viewport, camera)))
    {
      event->accept(Qt::SwipeGesture);
    }
  }
  else if (QGesture* pan = event->gesture(Qt::PanGesture))
  {
    if ((handled |= handlePanGesture(dynamic_cast<QPanGesture*>(pan), viewport, camera)))
    {
      event->accept(Qt::PanGesture);
    }
  }

  if (QGesture* pinch = event->gesture(Qt::PinchGesture))
  {
    if ((handled |= handlePinchGesture(dynamic_cast<QPinchGesture*>(pinch), viewport, camera)))
    {
      event->accept(Qt::PinchGesture);
    }
  }

  if (QGesture* tap = event->gesture(Qt::TapGesture))
  {
    if ((handled |= handleTapGesture(dynamic_cast<QTapGesture*>(tap), viewport, camera)))
    {
      event->accept(Qt::TapGesture);
    }
  }

  if (QGesture* tapAndHold = event->gesture(Qt::TapAndHoldGesture))
  {
    if ((handled
         |= handleTapAndHoldGesture(dynamic_cast<QTapAndHoldGesture*>(tapAndHold), viewport, camera)
        ))
    {
      event->accept(Qt::TapGesture);
    }
  }

  viewUpdater(handled);

  return handled;
}

bool InteractionHandlerBase::handlePanGesture(
  QPanGesture* gesture, const Viewport& viewport, const camera::Camera& camera
)
{
  if (!gesture)
  {
    return false;
  }

  bool handled = doHandlePanGesture(gesture, viewport, camera);
  viewUpdater(handled);

  return handled;
}

bool InteractionHandlerBase::handlePinchGesture(
  QPinchGesture* gesture, const Viewport& viewport, const camera::Camera& camera
)
{
  if (!gesture)
  {
    return false;
  }

  bool handled = doHandlePinchGesture(gesture, viewport, camera);
  viewUpdater(handled);

  return handled;
}

bool InteractionHandlerBase::handleSwipeGesture(
  QSwipeGesture* gesture, const Viewport& viewport, const camera::Camera& camera
)
{
  if (!gesture)
  {
    return false;
  }

  bool handled = doHandleSwipeGesture(gesture, viewport, camera);
  viewUpdater(handled);

  return handled;
}

bool InteractionHandlerBase::handleTapGesture(
  QTapGesture* gesture, const Viewport& viewport, const camera::Camera& camera
)
{
  if (!gesture)
  {
    return false;
  }

  bool handled = doHandleTapGesture(gesture, viewport, camera);
  viewUpdater(handled);

  return handled;
}

bool InteractionHandlerBase::handleTapAndHoldGesture(
  QTapAndHoldGesture* gesture, const Viewport& viewport, const camera::Camera& camera
)
{
  if (!gesture)
  {
    return false;
  }

  bool handled = doHandleTapAndHoldGesture(gesture, viewport, camera);
  viewUpdater(handled);

  return handled;
}

void InteractionHandlerBase::setUpdatesViewsOnEventHandled(bool doUpdate)
{
  m_updatesViewsOnEventHandled = doUpdate;
}

void InteractionHandlerBase::viewUpdater(bool eventHandled)
{
  if (m_updatesViewsOnEventHandled && eventHandled)
  {
    /// @todo Substitute this with explicit rules for when to update all views versus just this view:
    if (m_allViewsUpdater)
    {
      m_allViewsUpdater();
    }
    else if (m_myViewUpdater)
    {
      m_myViewUpdater();
    }
  }
}
