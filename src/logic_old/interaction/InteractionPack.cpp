#include "logic/interaction/InteractionPack.h"

#include "logic/camera/Camera.h"
#include "logic/interaction/CameraInteractionHandler.h"
#include "logic/interaction/CrosshairsInteractionHandler.h"
#include "logic/interaction/RefImageInteractionHandler.h"
#include "logic/interaction/SlideInteractionHandler.h"
#include "logic/interaction/StackInteractionHandler.h"
#include "logic/interaction/WindowLevelInteractionHandler.h"

InteractionPack::InteractionPack(
  const gui::ViewType& viewType,
  std::unique_ptr<camera::Camera> camera,
  std::unique_ptr<CameraInteractionHandler> cameraHandler,
  std::unique_ptr<CrosshairsInteractionHandler> crosshairsHandler,
  std::unique_ptr<RefImageInteractionHandler> refImageHandler,
  std::unique_ptr<SlideStackInteractionHandler> stackHandler,
  std::unique_ptr<SlideInteractionHandler> slideHandler,
  std::unique_ptr<WindowLevelInteractionHandler> wlHandler
)
  : m_viewType(viewType)
  , m_camera(std::move(camera))
  , m_cameraHandler(std::move(cameraHandler))
  , m_crosshairsHandler(std::move(crosshairsHandler))
  , m_refImageHandler(std::move(refImageHandler))
  , m_stackHandler(std::move(stackHandler))
  , m_slideHandler(std::move(slideHandler))
  , m_windowLevelHandler(std::move(wlHandler))
  , m_activeHandler(nullptr)
{
  setActiveHandlerType(InteractionHandlerType::Crosshairs);
}

//InteractionPack::~InteractionPack() = default;

const gui::ViewType& InteractionPack::getViewType() const
{
  return m_viewType;
}

camera::Camera* InteractionPack::getCamera()
{
  return m_camera.get();
}

CameraInteractionHandler* InteractionPack::getCameraHandler()
{
  return m_cameraHandler.get();
}

RefImageInteractionHandler* InteractionPack::getRefImageHandler()
{
  return m_refImageHandler.get();
}

CrosshairsInteractionHandler* InteractionPack::getCrosshairsHandler()
{
  return m_crosshairsHandler.get();
}

SlideStackInteractionHandler* InteractionPack::getStackHandler()
{
  return m_stackHandler.get();
}

SlideInteractionHandler* InteractionPack::getSlideHandler()
{
  return m_slideHandler.get();
}

WindowLevelInteractionHandler* InteractionPack::getWindowLevelHandler()
{
  return m_windowLevelHandler.get();
}

IInteractionHandler* InteractionPack::getActiveHandler()
{
  return m_activeHandler;
}

void InteractionPack::setActiveHandlerType(const InteractionHandlerType& type)
{
  switch (type)
  {
  case InteractionHandlerType::Camera:
  {
    m_activeHandler = m_cameraHandler.get();
    break;
  }
  case InteractionHandlerType::Crosshairs:
  {
    m_activeHandler = m_crosshairsHandler.get();
    break;
  }
  case InteractionHandlerType::RefImageTransform:
  {
    m_activeHandler = m_refImageHandler.get();
    break;
  }
  case InteractionHandlerType::SlideTransform:
  {
    m_activeHandler = m_slideHandler.get();
    break;
  }
  case InteractionHandlerType::StackTransform:
  {
    m_activeHandler = m_stackHandler.get();
    break;
  }
  case InteractionHandlerType::WindowLevel:
  {
    m_activeHandler = m_windowLevelHandler.get();
    break;
  }
  }
}
