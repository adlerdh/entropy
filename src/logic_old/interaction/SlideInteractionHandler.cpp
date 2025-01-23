#include "logic/interaction/SlideInteractionHandler.h"
#include "common/CoordinateFrame.h"
#include "logic/camera/CameraHelpers.h"

#include "slideio/SlideHelper.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_inverse.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/component_wise.hpp>

SlideInteractionHandler::SlideInteractionHandler()
  : InteractionHandlerBase(InteractionHandlerType::SlideTransform)
  ,

  m_stackFrameProvider(nullptr)
  , m_activeSlideProvider(nullptr)
  , m_slideTxChangedBroadcaster(nullptr)
  ,

  m_primaryMode(SlideInteractionMode::Rotate)
  , m_mouseMoveMode(MouseMoveMode::None)
  ,

  m_ndcLeftButtonStartPos(0.0f)
  , m_ndcRightButtonStartPos(0.0f)
  , m_ndcMiddleButtonStartPos(0.0f)
  , m_ndcLeftButtonLastPos(0.0f)
  , m_ndcRightButtonLastPos(0.0f)
  , m_ndcMiddleButtonLastPos(0.0f)
{
  // Do not update views when this class handles events. Instead, updates will be handled by
  // m_slideTxChangedBroadcaster
  setUpdatesViewsOnEventHandled(false);
}

void SlideInteractionHandler::setSlideStackFrameProvider(GetterType<CoordinateFrame> provider)
{
  m_stackFrameProvider = provider;
}

void SlideInteractionHandler::setActiveSlideRecordProvider(
  GetterType<std::weak_ptr<SlideRecord> > provider
)
{
  m_activeSlideProvider = provider;
}

void SlideInteractionHandler::setSlideTxsChangedBroadcaster(
  SetterType<const std::map<UID, slideio::SlideTransformation>&> broadcaster
)
{
  m_slideTxChangedBroadcaster = broadcaster;
}

void SlideInteractionHandler::setMode(const SlideInteractionMode& mode)
{
  m_primaryMode = mode;
  m_mouseMoveMode = MouseMoveMode::None;
}

bool SlideInteractionHandler::
  doHandleMouseDoubleClickEvent(const QMouseEvent*, const Viewport&, const camera::Camera&)
{
  return false;
}

bool SlideInteractionHandler::doHandleMouseMoveEvent(
  const QMouseEvent* event, const Viewport& viewport, const camera::Camera& camera
)
{
  bool handled = false;

  if (MouseMoveMode::None == m_mouseMoveMode)
  {
    return handled;
  }

  if (!m_activeSlideProvider || !m_stackFrameProvider)
  {
    return handled;
  }

  auto activeSlideRecord = m_activeSlideProvider().lock();
  if (!activeSlideRecord || !activeSlideRecord->cpuData())
  {
    return handled;
  }

  const slideio::SlideCpuRecord* slideCpuRecord = activeSlideRecord->cpuData();
  const glm::mat4 stackFrame_O_slide = slideio::stack_O_slide(*slideCpuRecord);
  slideio::SlideTransformation slideTx = slideCpuRecord->transformation();
  const CoordinateFrame stackFrame = m_stackFrameProvider();

  const glm::vec2 ndcPos = camera::ndc2d_O_mouse(viewport, {event->x(), event->y()});

  const bool shiftModifier = (Qt::ShiftModifier & event->modifiers());

  if (Qt::LeftButton & event->buttons())
  {
    switch (m_mouseMoveMode)
    {
    case MouseMoveMode::RotateZ:
    {
      const glm::vec3 worldStackAxis = camera::worldDirection(stackFrame, Directions::Cartesian::Z);
      const glm::mat4 world_O_slide = stackFrame.world_O_frame() * stackFrame_O_slide;
      const glm::vec4 c = world_O_slide
                          * glm::vec4{slideTx.normalizedRotationCenterXY(), 0.5f, 1.0f};
      const glm::vec3 worldRotationCenter = glm::vec3{c / c.w};
      const float ndcZ = ndcZofWorldPoint(camera, worldRotationCenter);

      const float angleDegrees = rotationAngleAboutWorldAxis(
        camera, m_ndcLeftButtonLastPos, ndcPos, ndcZ, worldStackAxis, worldRotationCenter
      );

      slideTx.setRotationAngleZ(slideTx.rotationAngleZ() + angleDegrees);
      handled = true;
      break;
    }
    case MouseMoveMode::ScaleXY:
    {
      const glm::vec3 slideRotationCenter(slideTx.normalizedRotationCenterXY(), 0.5f);
      const float ndcZ = ndcZofWorldPoint(camera, slideRotationCenter);
      const glm::mat4 slide_O_world = glm::inverse(stackFrame_O_slide) * stackFrame.frame_O_world();

      glm::vec2 scaleDelta = scaleFactorsAboutWorldAxis(
        camera, m_ndcLeftButtonLastPos, ndcPos, ndcZ, slide_O_world, slideRotationCenter
      );

      if (shiftModifier)
      {
        float minScale = glm::compMin(scaleDelta);
        float maxScale = glm::compMax(scaleDelta);

        if (maxScale > 1.0f)
        {
          scaleDelta = glm::vec2(maxScale);
        }
        else
        {
          scaleDelta = glm::vec2(minScale);
        }
      }

      // To prevent flipping and making the slide too small:
      static const glm::vec2 sk_minScale(0.1f);
      static const glm::vec2 sk_maxScale(10.0f);

      if (glm::all(glm::greaterThan(scaleDelta, sk_minScale)) && glm::all(glm::lessThan(scaleDelta, sk_maxScale)))
      {
        slideTx.setScaleFactorsXY(slideTx.scaleFactorsXY() * scaleDelta);
        handled = true;
      }

      break;
    }
    case MouseMoveMode::ShearXY:
    {
      break;
    }
    case MouseMoveMode::TranslateXY:
    {
      const glm::vec3 worldStackAxis = camera::worldDirection(stackFrame, Directions::Cartesian::Z);
      const glm::mat4 world_O_slide = stackFrame.world_O_frame() * stackFrame_O_slide;
      const glm::vec3 worldSlideOrigin = world_O_slide[3] / world_O_slide[3].w;
      const float ndcZ = ndcZofWorldPoint(camera, worldSlideOrigin);

      const glm::vec3 worldDelta = worldTranslationPerpendicularToWorldAxis(
        camera, m_ndcLeftButtonLastPos, ndcPos, ndcZ, worldStackAxis
      );

      const glm::mat3 f_O_w = glm::inverseTranspose(glm::mat3{stackFrame.frame_O_world()});
      const glm::vec2 frameDelta = glm::vec2(f_O_w * worldDelta);

      slideTx = slideio::translateXyInStack(*slideCpuRecord, frameDelta);
      handled = true;
      break;
    }
    case MouseMoveMode::TranslateZ:
    {
      const glm::vec3 worldStackAxis = camera::worldDirection(stackFrame, Directions::Cartesian::Z);
      const glm::mat4 world_O_slide = stackFrame.world_O_frame() * stackFrame_O_slide;
      const glm::vec3 worldSlideOrigin = world_O_slide[3] / world_O_slide[3].w;
      const float ndcZ = ndcZofWorldPoint(camera, worldSlideOrigin);

      const float axisDelta
        = axisTranslationAlongWorldAxis(camera, m_ndcLeftButtonLastPos, ndcPos, ndcZ, worldStackAxis);

      slideTx.setStackTranslationZ(slideTx.stackTranslationZ() + axisDelta);
      handled = true;
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
    m_ndcRightButtonLastPos = ndcPos;
  }
  else if (Qt::MiddleButton & event->buttons())
  {
    m_ndcMiddleButtonLastPos = ndcPos;
  }

  if (handled && m_slideTxChangedBroadcaster)
  {
    // Map of updated slide transformations following the user interaction
    std::map<UID, slideio::SlideTransformation> updatedSlideTxs;
    updatedSlideTxs.insert({activeSlideRecord->uid(), slideTx});
    m_slideTxChangedBroadcaster(updatedSlideTxs);
  }

  return handled;
}

bool SlideInteractionHandler::
  doHandleMousePressEvent(const QMouseEvent* event, const Viewport& viewport, const camera::Camera&)
{
  bool handled = false;

  const glm::vec2 ndcPos = camera::ndc2d_O_mouse(viewport, {event->x(), event->y()});

  const bool controlModifier = (Qt::ControlModifier & event->modifiers());

  if (Qt::LeftButton & event->button())
  {
    m_ndcLeftButtonStartPos = ndcPos;
    m_ndcLeftButtonLastPos = ndcPos;

    switch (m_primaryMode)
    {
    case SlideInteractionMode::Rotate:
    {
      m_mouseMoveMode = MouseMoveMode::RotateZ;
      handled = true;
      break;
    }
    case SlideInteractionMode::Stretch:
    {
      m_mouseMoveMode = (controlModifier) ? MouseMoveMode::ShearXY : MouseMoveMode::ScaleXY;
      handled = true;
      break;
    }
    case SlideInteractionMode::Translate:
    {
      m_mouseMoveMode = (controlModifier) ? MouseMoveMode::TranslateZ : MouseMoveMode::TranslateXY;
      handled = true;
      break;
    }
    }
  }
  else if (Qt::RightButton & event->button())
  {
    m_ndcRightButtonStartPos = ndcPos;
    m_ndcRightButtonLastPos = ndcPos;

    switch (m_primaryMode)
    {
    case SlideInteractionMode::Rotate:
    {
      m_mouseMoveMode = MouseMoveMode::None;
      handled = true;
      break;
    }
    case SlideInteractionMode::Stretch:
    {
      m_mouseMoveMode = MouseMoveMode::None;
      handled = true;
      break;
    }
    case SlideInteractionMode::Translate:
    {
      m_mouseMoveMode = MouseMoveMode::None;
      handled = true;
      break;
    }
    }
  }
  else if (Qt::MiddleButton & event->button())
  {
    m_ndcMiddleButtonStartPos = ndcPos;
    m_ndcMiddleButtonLastPos = ndcPos;
  }

  return handled;
}

bool SlideInteractionHandler::
  doHandleMouseReleaseEvent(const QMouseEvent* event, const Viewport& viewport, const camera::Camera&)
{
  bool handled = false;

  const glm::vec2 ndcPos = camera::ndc2d_O_mouse(viewport, {event->x(), event->y()});

  if (Qt::LeftButton == event->button())
  {
    m_ndcLeftButtonLastPos = ndcPos;
  }
  else if (Qt::RightButton == event->button())
  {
    m_ndcRightButtonLastPos = ndcPos;
  }
  else if (Qt::MiddleButton == event->button())
  {
    m_ndcMiddleButtonLastPos = ndcPos;
  }

  m_mouseMoveMode = MouseMoveMode::None;
  handled = true;

  return handled;
}
