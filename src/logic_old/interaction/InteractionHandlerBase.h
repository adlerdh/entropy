#ifndef INTERACTION_HANDLER_BASE_H
#define INTERACTION_HANDLER_BASE_H

#include "common/PublicTypes.h"
#include "logic/interfaces/IInteractionHandler.h"

#include <functional>

class InteractionHandlerBase : public IInteractionHandler
{
  using MyViewUpdater = std::function<void(void)>;

public:
  explicit InteractionHandlerBase(const InteractionHandlerType&);
  virtual ~InteractionHandlerBase() override = default;

  const InteractionHandlerType& type() const override;

  void setAllViewsUpdater(AllViewsUpdaterType);
  void setMyViewUpdater(MyViewUpdater);

  bool handleMouseDoubleClickEvent(QMouseEvent*, const Viewport&, const Camera&) override;
  bool handleMouseMoveEvent(QMouseEvent*, const Viewport&, const Camera&) override;
  bool handleMousePressEvent(QMouseEvent*, const Viewport&, const Camera&) override;
  bool handleMouseReleaseEvent(QMouseEvent*, const Viewport&, const Camera&) override;

  bool handleTabletEvent(QTabletEvent*, const Viewport&, const Camera&) override;

  bool handleWheelEvent(QWheelEvent*, const Viewport&, const Camera&) override;

  bool dispatchGestureEvent(QGestureEvent*, const Viewport&, const Camera&) override;
  bool handlePanGesture(QPanGesture*, const Viewport&, const Camera&) override;
  bool handlePinchGesture(QPinchGesture*, const Viewport&, const Camera&) override;
  bool handleSwipeGesture(QSwipeGesture*, const Viewport&, const Camera&) override;
  bool handleTapGesture(QTapGesture*, const Viewport&, const Camera&) override;
  bool handleTapAndHoldGesture(QTapAndHoldGesture*, const Viewport&, const Camera&) override;

protected:
  virtual bool
  doHandleMouseDoubleClickEvent(const QMouseEvent*, const Viewport&, const Camera&)
    = 0;
  virtual bool doHandleMouseMoveEvent(const QMouseEvent*, const Viewport&, const Camera&) = 0;
  virtual bool doHandleMousePressEvent(const QMouseEvent*, const Viewport&, const Camera&)
    = 0;
  virtual bool doHandleMouseReleaseEvent(const QMouseEvent*, const Viewport&, const Camera&)
    = 0;

  virtual bool doHandleTabletEvent(const QTabletEvent*, const Viewport&, const Camera&) = 0;

  virtual bool doHandleWheelEvent(const QWheelEvent*, const Viewport&, const Camera&) = 0;

  virtual bool doHandlePanGesture(const QPanGesture*, const Viewport&, const Camera&) = 0;
  virtual bool doHandlePinchGesture(const QPinchGesture*, const Viewport&, const Camera&) = 0;
  virtual bool doHandleSwipeGesture(const QSwipeGesture*, const Viewport&, const Camera&) = 0;
  virtual bool doHandleTapGesture(const QTapGesture*, const Viewport&, const Camera&) = 0;
  virtual bool
  doHandleTapAndHoldGesture(const QTapAndHoldGesture*, const Viewport&, const Camera&)
    = 0;

  void setUpdatesViewsOnEventHandled(bool doUpdate);

  void viewUpdater(bool eventHandled);

  const InteractionHandlerType m_type;

  AllViewsUpdaterType m_allViewsUpdater;
  MyViewUpdater m_myViewUpdater;

  bool m_updatesViewsOnEventHandled;
};

#endif // INTERACTION_HANDLER_BASE_H
