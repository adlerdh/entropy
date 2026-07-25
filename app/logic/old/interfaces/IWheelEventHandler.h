#pragma once

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

  virtual bool handleWheelEvent(QWheelEvent*, const Viewport&, const Camera&) = 0;
};
