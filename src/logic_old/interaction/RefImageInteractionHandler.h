#ifndef REF_IMAGE_INTERACTION_HANDLER_H
#define REF_IMAGE_INTERACTION_HANDLER_H

#include "common/PublicTypes.h"
#include "logic/interaction/InteractionHandlerBase.h"
#include "logic/interaction/InteractionModes.h"

#include <glm/vec2.hpp>

#include <functional>
#include <optional>

class CoordinateFrame;

/**
 * @brief Handle transformation interactions with the active Reference Image
 */
class RefImageInteractionHandler : public InteractionHandlerBase
{
public:
  explicit RefImageInteractionHandler();

  ~RefImageInteractionHandler() override = default;

  /// Set function returning the crosshairs world origin
  void setCrosshairsOriginProvider(GetterType<glm::vec3>);

  /// Set function returning the frame mapping the active image Subject to World space.
  /// If there is no active image, std::nullopt is expected.
  void setImageFrameProvider(GetterType<std::optional<CoordinateFrame> >);

  /// Set function for broadcasting a non-final change to the frame mapping active image
  /// Subject to World space
  void setImageFrameChangedBroadcaster(SetterType<const CoordinateFrame&>);

  /// Set function for broadcasting a final change to the frame mapping active image
  /// Subject to World space
  void setImageFrameChangeDoneBroadcaster(SetterType<const CoordinateFrame&>);

  /// Set function returning the World-space diagonal voxel length of the reference image
  void setImageVoxelScaleProvider(GetterType<float>);

  /// Set the interaction mode
  void setMode(const RefImageInteractionMode&);

private:
  enum class MouseMoveMode
  {
    TranslateInPlane,
    TranslateFrontBack,
    Rotate2dInPlane,
    Rotate3dAboutPlane,
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

  /// Provides the World-space crosshairs origin
  GetterType<glm::vec3> m_crosshairsOriginProvider;

  /// Provides the coordinate frame of the image
  GetterType<std::optional<CoordinateFrame> > m_imageFrameProvider;

  /// Broadcasts that the image coordinate frame changed
  SetterType<const CoordinateFrame&> m_imageFrameChangedBroadcaster;

  /// Broadcasts that the image coordinate frame is done changing
  SetterType<const CoordinateFrame&> m_imageFrameDoneBroadcaster;

  /// Provides the image voxel scale size
  GetterType<float> m_imageVoxelScaleProvider;

  RefImageInteractionMode m_primaryMode;
  MouseMoveMode m_mouseMoveMode;

  glm::vec2 m_ndcLeftButtonStartPos;
  glm::vec2 m_ndcRightButtonStartPos;
  glm::vec2 m_ndcMiddleButtonStartPos;

  glm::vec2 m_ndcLeftButtonLastPos;
  glm::vec2 m_ndcRightButtonLastPos;
  glm::vec2 m_ndcMiddleButtonLastPos;
};

#endif // REF_IMAGE_INTERACTION_HANDLER_H
