#ifndef CROSSHAIRS_INTERACTION_HANDLER_H
#define CROSSHAIRS_INTERACTION_HANDLER_H

#include "logic/interaction/InteractionHandlerBase.h"
#include "logic/interaction/InteractionModes.h"

#include "common/PublicTypes.h"

#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include <functional>
#include <unordered_map>
#include <utility>

class CoordinateFrame;

class CrosshairsInteractionHandler : public InteractionHandlerBase
{
public:
  /// Function returning the NDC Z-depth at a 2D NDC position picked in a planar ("2D") view
  using PlanarPointPickerType = std::function<float(const glm::vec2& ndcPos)>;

  /// Function returning the NDC Z-depth and object ID at a 2D NDC position picked in a "3D" view
  using DepthPointPickerType = std::function<std::pair<uint16_t, float>(const glm::vec2& ndcPos)>;

  /// Function returning the distance by which to move the crosshairs origin on a "scroll"
  /// operation along a given World-space camera front axis
  using ScrollDistanceProviderType = std::function<float(const glm::vec3& worldCameraFront)>;

  explicit CrosshairsInteractionHandler();
  ~CrosshairsInteractionHandler() override = default;

  void setPlanarPointPicker(PlanarPointPickerType);
  void setDepthPointPicker(DepthPointPickerType);
  void setScrollDistanceProvider(ScrollDistanceProviderType);

  void setCrosshairsFrameProvider(GetterType<CoordinateFrame>);
  void setCrosshairsFrameChangedBroadcaster(SetterType<const CoordinateFrame&>);
  void setCrosshairsFrameChangeDoneBroadcaster(SetterType<const CoordinateFrame&>);

  void setObjectIdBroadcaster(SetterType<uint16_t>);

  void setPointPickingMode(const CrosshairsPointPickingMode&);

  void setMode(const CrosshairsInteractionMode&);

  void setRotationModeEnabled(bool enabled);

private:
  enum class MouseMoveMode
  {
    Translate,
    RotateInPlane,
    RotateAboutOrigin,
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

  bool doHandleWheelEvent(const QWheelEvent*, const Viewport&, const camera::Camera&) override;

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

  bool moveToObjectAtNdcPosition(const camera::Camera&, const glm::vec2& ndcPosXY);

  PlanarPointPickerType m_planarPointPicker;
  DepthPointPickerType m_depthPointPicker;
  ScrollDistanceProviderType m_scrollDistanceProvider;

  GetterType<CoordinateFrame> m_crosshairsFrameProvider;
  SetterType<const CoordinateFrame&> m_crosshairsFrameChangedBroadcaster;
  SetterType<const CoordinateFrame&> m_crosshairsFrameChangeDoneBroadcaster;
  SetterType<uint16_t> m_objectIdBroadcaster;

  CrosshairsPointPickingMode m_pointPickingMode;

  CrosshairsInteractionMode m_primaryMode;
  MouseMoveMode m_mouseMoveMode;

  /// Flag to enable/disable crosshairs rotation mode
  bool m_rotationModeEnabled;

  glm::vec2 m_ndcLeftButtonStartPos;
  glm::vec2 m_ndcRightButtonStartPos;
  //    glm::vec2 m_ndcMiddleButtonStartPos;

  glm::vec2 m_ndcLeftButtonLastPos;
  glm::vec2 m_ndcRightButtonLastPos;
  //    glm::vec2 m_ndcMiddleButtonLastPos;

  const static std::unordered_map<CrosshairsInteractionMode, MouseMoveMode>
    msk_defaultInternalModeMap;
};

#endif // CROSSHAIRS_INTERACTION_HANDLER_H
