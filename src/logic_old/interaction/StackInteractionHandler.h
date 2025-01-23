#ifndef STACK_INTERACTION_HANDLER_H
#define STACK_INTERACTION_HANDLER_H

#include "common/PublicTypes.h"
#include "logic/interaction/InteractionHandlerBase.h"
#include "logic/interaction/InteractionModes.h"

#include <glm/vec2.hpp>

#include <functional>

class CoordinateFrame;

/**
 * @brief Handle interaction with the entire Slide Stack
 */
class SlideStackInteractionHandler : public InteractionHandlerBase
{
public:
  explicit SlideStackInteractionHandler();

  ~SlideStackInteractionHandler() override = default;

  void setSlideStackFrameProvider(GetterType<CoordinateFrame>);
  void setSlideStackFrameChangedBroadcaster(SetterType<const CoordinateFrame&>);
  void setSlideStackFrameChangeDoneBroadcaster(SetterType<const CoordinateFrame&>);

  /// Set function returning the World-space diagonal voxel length of the reference image
  void setRefImageVoxelScaleProvider(GetterType<float>);

  void setMode(const StackInteractionMode&);

private:
  enum class MouseMoveMode
  {
    TranslateInPlane,
    TranslateFrontBack,
    Rotate2DInPlane,
    Rotate3DAboutPlane,
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

  /// Provides the slide stack frame
  GetterType<CoordinateFrame> m_stackFrameProvider;

  /// Broadcasts that the slide stack frame changed
  SetterType<const CoordinateFrame&> m_stackFrameChangedBroadcaster;

  /// Broadcasts that the slide stack frame is done changing
  SetterType<const CoordinateFrame&> m_stackFrameDoneBroadcaster;

  /// Provides the reference image voxel scale size
  GetterType<float> m_activeImageVoxelScaleProvider;

  StackInteractionMode m_primaryMode;
  MouseMoveMode m_mouseMoveMode;

  glm::vec2 m_ndcLeftButtonStartPos;
  glm::vec2 m_ndcRightButtonStartPos;
  glm::vec2 m_ndcMiddleButtonStartPos;

  glm::vec2 m_ndcLeftButtonLastPos;
  glm::vec2 m_ndcRightButtonLastPos;
  glm::vec2 m_ndcMiddleButtonLastPos;
};

#endif // STACK_INTERACTION_HANDLER_H
