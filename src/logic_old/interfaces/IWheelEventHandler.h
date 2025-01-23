#ifndef I_WHEEL_EVENT_HANDLER_H
#define I_WHEEL_EVENT_HANDLER_H

#include "common/Viewport.h"
#include "logic/camera/Camera.h"

#include <QWheelEvent>

/**
 * @brief Interface for wheel event handling
 */
class IWheelEventHandler
{
public:
  virtual ~IWheelEventHandler() = default;

  virtual bool handleWheelEvent(QWheelEvent*, const Viewport&, const camera::Camera&) = 0;
};

#endif // I_WHEEL_EVENT_HANDLER_H
