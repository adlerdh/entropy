#ifndef SLIDE_INTERACTION_HANDLER_H
#define SLIDE_INTERACTION_HANDLER_H

#include "logic/interaction/InteractionHandlerBase.h"
#include "logic/interaction/InteractionModes.h"
#include "logic/records/SlideRecord.h"

#include "common/UID.h"

#include <glm/vec2.hpp>

#include <map>

class CoordinateFrame;

namespace slideio
{
class SlideTransformation;
}

class SlideInteractionHandler : public InteractionHandlerBase
{
public:
  explicit SlideInteractionHandler();
  ~SlideInteractionHandler() override = default;

  /// Set function returning the Slide Stack coordinate frame
  void setSlideStackFrameProvider(GetterType<CoordinateFrame>);

  /// Set function returning the record of the active slide, if one exists.
  void setActiveSlideRecordProvider(GetterType<std::weak_ptr<SlideRecord> >);

  /// Set function for broadcasting that the transformations of slides have changed.
  /// The argument is a map of slide UID to updated SlideTransformation.
  void setSlideTxsChangedBroadcaster(SetterType<const std::map<UID, slideio::SlideTransformation>&>);

  void setMode(const SlideInteractionMode&);

private:
  enum class MouseMoveMode
  {
    RotateZ,
    ScaleXY,
    ShearXY,
    TranslateXY,
    TranslateZ,
    None
  };

  bool doHandleMouseDoubleClickEvent(const QMouseEvent*, const Viewport&, const camera::Camera&)
    override;
  bool doHandleMouseMoveEvent(const QMouseEvent*, const Viewport&, const camera::Camera&) override;
  bool doHandleMousePressEvent(const QMouseEvent*, const Viewport&, const camera::Camera&) override;
  bool doHandleMouseReleaseEvent(const QMouseEvent*, const Viewport&, const camera::Camera&) override;

  bool doHandleTabletEvent(const QTabletEvent*, const Viewport&, const camera::Camera&) override
  {
    return false;
  }

  bool doHandleWheelEvent(const QWheelEvent*, const Viewport&, const camera::Camera&) override
  {
    return false;
  }

  bool doHandlePanGesture(const QPanGesture*, const Viewport&, const camera::Camera&) override
  {
    return false;
  }
  bool doHandlePinchGesture(const QPinchGesture*, const Viewport&, const camera::Camera&) override
  {
    return false;
  }
  bool doHandleSwipeGesture(const QSwipeGesture*, const Viewport&, const camera::Camera&) override
  {
    return false;
  }
  bool doHandleTapGesture(const QTapGesture*, const Viewport&, const camera::Camera&) override
  {
    return false;
  }
  bool doHandleTapAndHoldGesture(const QTapAndHoldGesture*, const Viewport&, const camera::Camera&)
    override
  {
    return false;
  }

  GetterType<CoordinateFrame> m_stackFrameProvider;
  GetterType<std::weak_ptr<SlideRecord> > m_activeSlideProvider;
  SetterType<const std::map<UID, slideio::SlideTransformation>&> m_slideTxChangedBroadcaster;

  SlideInteractionMode m_primaryMode;
  MouseMoveMode m_mouseMoveMode;

  glm::vec2 m_ndcLeftButtonStartPos;
  glm::vec2 m_ndcRightButtonStartPos;
  glm::vec2 m_ndcMiddleButtonStartPos;

  glm::vec2 m_ndcLeftButtonLastPos;
  glm::vec2 m_ndcRightButtonLastPos;
  glm::vec2 m_ndcMiddleButtonLastPos;
};

#endif // SLIDE_INTERACTION_HANDLER_H
