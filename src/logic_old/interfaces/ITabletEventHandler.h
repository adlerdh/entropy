#ifndef I_TABLET_EVENT_HANDLER_H
#define I_TABLET_EVENT_HANDLER_H

#include "common/Viewport.h"
#include "logic/camera/Camera.h"

#include <QTabletEvent>

/// @todo These are more complex gestures that we could handle in the future:
//    Qt::BeginNativeGesture
//    Qt::EndNativeGesture
//    Qt::PanNativeGesture
//    Qt::ZoomNativeGesture
//    Qt::SmartZoomNativeGesture
//    Qt::RotateNativeGesture
//    Qt::SwipeNativeGesture

/**
 * @brief Interface for tablet event handling
 */
class ITabletEventHandler
{
public:
  virtual ~ITabletEventHandler() = default;

  virtual bool handleTabletEvent(QTabletEvent*, const Viewport&, const camera::Camera&) = 0;
};

#endif // I_TABLET_EVENT_HANDLER_H
