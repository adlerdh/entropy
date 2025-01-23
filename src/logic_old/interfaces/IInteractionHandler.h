#ifndef I_INTERACTION_HANDLER_H
#define I_INTERACTION_HANDLER_H

#include "logic/interfaces/IGestureHandler.h"
#include "logic/interfaces/IMouseEventHandler.h"
#include "logic/interfaces/ITabletEventHandler.h"
#include "logic/interfaces/IWheelEventHandler.h"

#include "logic/interaction/InteractionHandlerType.h"

/**
  @note other kinds of events that we might want to handle:
    QInputEvent:
        QContextMenuEvent,
        QHoverEvent,
        QKeyEvent,
        QNativeGestureEvent,
        QTouchEvent,
*/

class QGestureEvent;

/**
 * @brief Interface for all input event handling
 */
class IInteractionHandler : public IGestureHandler,
                            public IMouseEventHandler,
                            public ITabletEventHandler,
                            public IWheelEventHandler
{
public:
  virtual ~IInteractionHandler() = default;

  virtual const InteractionHandlerType& type() const = 0;

  /// Dispatch handling of the gesture event, since it may be either a
  /// swipe, pan, pinch, tap, or tap-and-hold event
  virtual bool dispatchGestureEvent(QGestureEvent*, const Viewport&, const camera::Camera&) = 0;
};

#endif // I_INTERACTION_HANDLER_H
