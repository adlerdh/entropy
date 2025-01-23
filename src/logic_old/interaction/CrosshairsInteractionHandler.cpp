#include "logic/interaction/CrosshairsInteractionHandler.h"
#include "common/CoordinateFrame.h"
#include "logic/camera/CameraHelpers.h"

#include "rendering/common/ShaderStageTypes.h"
#include "rendering/utility/UnderlyingEnumType.h"

#include <glm/glm.hpp>

#include <tuple>

namespace
{

// Distance in World-space units by which to nudge 3D point pick results on meshes into the scene:
/// @note This could be useful, but has been set to zero (i.e. no nudging) for now
static constexpr float sk_worldNudge = 0.0f; //1.0e-1f;

} // namespace

const std::unordered_map<CrosshairsInteractionMode, CrosshairsInteractionHandler::MouseMoveMode>
  CrosshairsInteractionHandler::msk_defaultInternalModeMap = {
    {CrosshairsInteractionMode::Move, CrosshairsInteractionHandler::MouseMoveMode::Translate}
};

CrosshairsInteractionHandler::CrosshairsInteractionHandler()
  : InteractionHandlerBase(InteractionHandlerType::Crosshairs)
  ,

  m_planarPointPicker(nullptr)
  , m_depthPointPicker(nullptr)
  , m_scrollDistanceProvider(nullptr)
  ,

  m_crosshairsFrameProvider(nullptr)
  , m_crosshairsFrameChangedBroadcaster(nullptr)
  , m_crosshairsFrameChangeDoneBroadcaster(nullptr)
  ,

  m_objectIdBroadcaster(nullptr)
  ,

  m_pointPickingMode(CrosshairsPointPickingMode::DepthPicking)
  ,

  m_primaryMode(CrosshairsInteractionMode::Move)
  ,

  m_rotationModeEnabled(true)
  ,

  m_ndcLeftButtonStartPos(0.0f)
  , m_ndcRightButtonStartPos(0.0f)
  ,
  //      m_ndcMiddleButtonStartPos( 0.0f ),
  m_ndcLeftButtonLastPos(0.0f)
  , m_ndcRightButtonLastPos(0.0f)
//      m_ndcMiddleButtonLastPos( 0.0f )
{
  // Do not update views when this class handles events. Instead, updates will be handled by
  // m_crosshairsFrameChangedBroadcaster and m_crosshairsFrameChangeDoneBroadcaster
  setUpdatesViewsOnEventHandled(false);

  m_mouseMoveMode = msk_defaultInternalModeMap.at(m_primaryMode);
}

void CrosshairsInteractionHandler::setPlanarPointPicker(PlanarPointPickerType picker)
{
  m_planarPointPicker = picker;
}

void CrosshairsInteractionHandler::setDepthPointPicker(DepthPointPickerType picker)
{
  m_depthPointPicker = picker;
}

void CrosshairsInteractionHandler::setScrollDistanceProvider(ScrollDistanceProviderType provider)
{
  m_scrollDistanceProvider = provider;
}

void CrosshairsInteractionHandler::setCrosshairsFrameProvider(GetterType<CoordinateFrame> requester)
{
  m_crosshairsFrameProvider = requester;

  // Send back a first notification of the crosshairs world position
  //    if ( m_crosshairsFrameProvider && m_crosshairsFrameChangedDoneBroadcaster )
  //    {
  //        m_crosshairsFrameChangedDoneBroadcaster( m_crosshairsFrameProvider() );
  //    }
}

void CrosshairsInteractionHandler::setCrosshairsFrameChangedBroadcaster(
  SetterType<const CoordinateFrame&> broadcaster
)
{
  m_crosshairsFrameChangedBroadcaster = broadcaster;
}

void CrosshairsInteractionHandler::setCrosshairsFrameChangeDoneBroadcaster(
  SetterType<const CoordinateFrame&> broadcaster
)
{
  m_crosshairsFrameChangeDoneBroadcaster = broadcaster;
}

void CrosshairsInteractionHandler::setObjectIdBroadcaster(SetterType<uint16_t> broadcaster)
{
  m_objectIdBroadcaster = broadcaster;
}

void CrosshairsInteractionHandler::setPointPickingMode(const CrosshairsPointPickingMode& mode)
{
  m_pointPickingMode = mode;
}

void CrosshairsInteractionHandler::setMode(const CrosshairsInteractionMode& mode)
{
  m_primaryMode = mode;
  m_mouseMoveMode = msk_defaultInternalModeMap.at(m_primaryMode);
}

void CrosshairsInteractionHandler::setRotationModeEnabled(bool enabled)
{
  m_rotationModeEnabled = enabled;
}

bool CrosshairsInteractionHandler::moveToObjectAtNdcPosition(
  const camera::Camera& camera, const glm::vec2& ndcPosXY
)
{
  bool handled = false;

  if (!m_crosshairsFrameProvider || !m_crosshairsFrameChangedBroadcaster)
  {
    return false;
  }

  uint16_t objectId;
  float ndcZ;

  switch (m_pointPickingMode)
  {
  case CrosshairsPointPickingMode::PlanarPicking:
  {
    if (!m_planarPointPicker)
    {
      return handled;
    }

    ndcZ = m_planarPointPicker(ndcPosXY);
    objectId = 1u;
    break;
  }
  case CrosshairsPointPickingMode::DepthPicking:
  {
    if (!m_depthPointPicker)
    {
      return handled;
    }

    std::tie(objectId, ndcZ) = m_depthPointPicker(ndcPosXY);
    break;
  }
  }

  /// @todo make filtering dependent on view, so that different views can select different objects
  if (0 < objectId)
  {
    const glm::vec3 ndcPos{ndcPosXY.x, ndcPosXY.y, ndcZ};
    glm::vec3 worldPos = world_O_ndc(camera, ndcPos);

    const uint32_t meshType = static_cast<uint32_t>(underlyingType(DrawableType::TexturedMesh));

    if ((objectId >> 12) & meshType)
    {
      // If set hit a mesh, nudge the point a little deeper into the scene.
      const glm::vec3 nudge = sk_worldNudge
                              * camera::worldDirection(camera, Directions::View::Front);
      worldPos += nudge;
    }

    auto crosshairsFrame = m_crosshairsFrameProvider();
    crosshairsFrame.setWorldOrigin(worldPos);
    m_crosshairsFrameChangedBroadcaster(crosshairsFrame);

    handled = true;
  }

  if (m_objectIdBroadcaster)
  {
    m_objectIdBroadcaster(objectId);
  }

  return handled;
}

bool CrosshairsInteractionHandler::
  doHandleMouseDoubleClickEvent(const QMouseEvent*, const Viewport&, const camera::Camera&)
{
  return false;
}

bool CrosshairsInteractionHandler::doHandleMouseMoveEvent(
  const QMouseEvent* event, const Viewport& viewport, const camera::Camera& camera
)
{
  if (MouseMoveMode::None == m_mouseMoveMode)
  {
    return false;
  }

  if (!m_crosshairsFrameProvider || !m_crosshairsFrameChangedBroadcaster)
  {
    return false;
  }

  bool handled = false;

  const glm::vec2 ndcPos = camera::ndc2d_O_mouse(viewport, {event->x(), event->y()});

  if (Qt::LeftButton & event->buttons())
  {
    switch (m_mouseMoveMode)
    {
    case MouseMoveMode::Translate:
    {
      handled = moveToObjectAtNdcPosition(camera, ndcPos);
      break;
    }
    case MouseMoveMode::RotateInPlane:
    {
      break;
    }
    case MouseMoveMode::RotateAboutOrigin:
    {
      break;
    }
    case MouseMoveMode::None:
    {
      break;
    }
    }

    m_ndcLeftButtonLastPos = ndcPos;
  }
  else if (Qt::RightButton & event->buttons())
  {
    switch (m_mouseMoveMode)
    {
    case MouseMoveMode::Translate:
    {
      break;
    }
    case MouseMoveMode::RotateInPlane:
    {
      auto crosshairs = m_crosshairsFrameProvider();
      const glm::vec2 ndcRotationCenter = ndc_O_world(camera, crosshairs.worldOrigin());
      const glm::quat R
        = rotation2dInCameraPlane(camera, m_ndcRightButtonLastPos, ndcPos, ndcRotationCenter);

      crosshairs.setFrameToWorldRotation(R * crosshairs.world_O_frame_rotation());
      m_crosshairsFrameChangedBroadcaster(crosshairs);

      handled = true;
      break;
    }
    case MouseMoveMode::RotateAboutOrigin:
    {
      auto crosshairs = m_crosshairsFrameProvider();
      const glm::quat R = rotation3dAboutCameraPlane(camera, m_ndcRightButtonLastPos, ndcPos);

      crosshairs.setFrameToWorldRotation(R * crosshairs.world_O_frame_rotation());
      m_crosshairsFrameChangedBroadcaster(crosshairs);

      handled = true;
      break;
    }
    case MouseMoveMode::None:
    {
      break;
    }
    }

    m_ndcRightButtonLastPos = ndcPos;
  }

  return handled;
}

bool CrosshairsInteractionHandler::doHandleMousePressEvent(
  const QMouseEvent* event, const Viewport& viewport, const camera::Camera& camera
)
{
  bool handled = false;

  const glm::vec2 ndcPos = camera::ndc2d_O_mouse(viewport, {event->x(), event->y()});

  const bool controlModifier = (Qt::ControlModifier & event->modifiers());
  //    const bool shiftModifier = ( Qt::ShiftModifier & event->modifiers() );

  if (Qt::LeftButton == event->button())
  {
    m_ndcLeftButtonStartPos = ndcPos;
    m_ndcLeftButtonLastPos = ndcPos;

    switch (m_primaryMode)
    {
    case CrosshairsInteractionMode::Move:
    {
      m_mouseMoveMode = MouseMoveMode::Translate;
      moveToObjectAtNdcPosition(camera, ndcPos);
      handled = true;
      break;
    }
    }
  }
  else if (Qt::RightButton == event->button())
  {
    m_ndcRightButtonStartPos = ndcPos;
    m_ndcRightButtonLastPos = ndcPos;

    switch (m_primaryMode)
    {
    case CrosshairsInteractionMode::Move:
    {
      if (m_rotationModeEnabled)
      {
        if (controlModifier)
        {
          m_mouseMoveMode = MouseMoveMode::RotateAboutOrigin;
        }
        else
        {
          m_mouseMoveMode = MouseMoveMode::RotateInPlane;
        }
      }
      handled = true;
      break;
    }
    }
  }

  return handled;
}

bool CrosshairsInteractionHandler::
  doHandleMouseReleaseEvent(const QMouseEvent* event, const Viewport& viewport, const camera::Camera&)
{
  if (!m_crosshairsFrameProvider || !m_crosshairsFrameChangeDoneBroadcaster)
  {
    return false;
  }

  bool handled = false;

  const glm::vec2 ndcPos = camera::ndc2d_O_mouse(viewport, {event->x(), event->y()});

  if (Qt::LeftButton == event->button())
  {
    m_ndcLeftButtonLastPos = ndcPos;
    m_mouseMoveMode = MouseMoveMode::None;

    // Commit the crosshairs frame, since mouse was released
    m_crosshairsFrameChangeDoneBroadcaster(m_crosshairsFrameProvider());
    handled = true;
  }
  else if (Qt::RightButton == event->button())
  {
    m_ndcRightButtonLastPos = ndcPos;
    m_mouseMoveMode = MouseMoveMode::None;

    // Commit the crosshairs frame, since mouse was released
    m_crosshairsFrameChangeDoneBroadcaster(m_crosshairsFrameProvider());
    handled = true;
  }
  //    else if ( Qt::MiddleButton & event->button() )
  //    {
  //        m_ndcMiddleButtonLastPos = ndcPos;
  //        handled = true;
  //    }

  return handled;
}

bool CrosshairsInteractionHandler::doHandleWheelEvent(
  const QWheelEvent* event, const Viewport&, const camera::Camera& camera
)
{
  //    event->orientation(); Qt::Orientation;
  //    event->phase(); Qt::ScrollPhase;

  bool handled = false;

  const float inv = (event->inverted()) ? -1.0f : 1.0f;
  const float numDegrees = event->angleDelta().y() / 8.0f;
  const float delta = inv * numDegrees / 15.0f;

  switch (m_primaryMode)
  {
  case CrosshairsInteractionMode::Move:
  {
    if (!m_scrollDistanceProvider || !m_crosshairsFrameProvider || !m_crosshairsFrameChangeDoneBroadcaster)
    {
      return handled;
    }

    // Crosshairs move in direction of the frustum ray passing through the
    // current crosshairs position. For an orthographic camera, this direction
    // is equivalent to worldDirection( camera, Directions::View::Front )
    auto crosshairsFrame = m_crosshairsFrameProvider();
    const glm::vec3 worldPos = crosshairsFrame.worldOrigin();
    const glm::vec3 cameraFront = worldRayDirection(camera, glm::vec2{ndc_O_world(camera, worldPos)});
    const float scrollDistance = m_scrollDistanceProvider(cameraFront);

    crosshairsFrame.setWorldOrigin(delta * scrollDistance * cameraFront + worldPos);
    m_crosshairsFrameChangeDoneBroadcaster(crosshairsFrame);

    handled = true;
    break;
  }
  }

  return handled;
}
