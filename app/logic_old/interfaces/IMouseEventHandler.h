#pragma once

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

  virtual bool handleMouseDoubleClickEvent(QMouseEvent*, const Viewport&, const Camera&) = 0;
  virtual bool handleMouseMoveEvent(QMouseEvent*, const Viewport&, const Camera&) = 0;
  virtual bool handleMousePressEvent(QMouseEvent*, const Viewport&, const Camera&) = 0;
  virtual bool handleMouseReleaseEvent(QMouseEvent*, const Viewport&, const Camera&) = 0;
};
