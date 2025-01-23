#ifndef I_MOUSE_EVENT_HANDLER_H
#define I_MOUSE_EVENT_HANDLER_H

#include "common/Viewport.h"
#include "logic/camera/Camera.h"

#include <QMouseEvent>

/**
 * @brief Interface for mouse event handling
 */
class IMouseEventHandler
{
public:
  virtual ~IMouseEventHandler() = default;

  virtual bool handleMouseDoubleClickEvent(QMouseEvent*, const Viewport&, const camera::Camera&) = 0;
  virtual bool handleMouseMoveEvent(QMouseEvent*, const Viewport&, const camera::Camera&) = 0;
  virtual bool handleMousePressEvent(QMouseEvent*, const Viewport&, const camera::Camera&) = 0;
  virtual bool handleMouseReleaseEvent(QMouseEvent*, const Viewport&, const camera::Camera&) = 0;
};

#endif // I_MOUSE_EVENT_HANDLER_H
