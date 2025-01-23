#ifndef I_GESTURE_EVENT_HANDLER_H
#define I_GESTURE_EVENT_HANDLER_H

#include "common/Viewport.h"
#include "logic/camera/Camera.h"

#include <QPanGesture>
#include <QPinchGesture>
#include <QSwipeGesture>
#include <QTapAndHoldGesture>
#include <QTapGesture>

/**
 * @brief Interface for gesture event handling
 */
class IGestureHandler
{
public:
  virtual ~IGestureHandler() = default;

  virtual bool handlePanGesture(QPanGesture*, const Viewport&, const camera::Camera&) = 0;
  virtual bool handlePinchGesture(QPinchGesture*, const Viewport&, const camera::Camera&) = 0;
  virtual bool handleSwipeGesture(QSwipeGesture*, const Viewport&, const camera::Camera&) = 0;
  virtual bool handleTapGesture(QTapGesture*, const Viewport&, const camera::Camera&) = 0;
  virtual bool handleTapAndHoldGesture(QTapAndHoldGesture*, const Viewport&, const camera::Camera&)
    = 0;
};

#endif // I_GESTURE_EVENT_HANDLER_H
