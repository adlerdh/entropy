#ifndef INTERACTION_PACK_H
#define INTERACTION_PACK_H

#include "gui/layout/ViewType.h"
#include "logic/interaction/InteractionHandlerType.h"

#include <memory>

namespace camera
{
class Camera;
}

class IInteractionHandler;
class CameraInteractionHandler;
class CrosshairsInteractionHandler;
class RefImageInteractionHandler;
class SlideStackInteractionHandler;
class SlideInteractionHandler;
class WindowLevelInteractionHandler;

/**
 * @brief Collection of all the objects related to interaction and event handling for a view.
 * This class owns the interaction objects.
 */
class InteractionPack
{
public:
  InteractionPack(
    const gui::ViewType& viewType,
    std::unique_ptr<camera::Camera> camera,
    std::unique_ptr<CameraInteractionHandler> cameraHandler,
    std::unique_ptr<CrosshairsInteractionHandler> crosshairsHandler,
    std::unique_ptr<RefImageInteractionHandler> refImageHandler,
    std::unique_ptr<SlideStackInteractionHandler> stackHandler,
    std::unique_ptr<SlideInteractionHandler> slideHandler,
    std::unique_ptr<WindowLevelInteractionHandler> wlHandler
  );

  ~InteractionPack() = default;

  /// Get the view type
  const gui::ViewType& getViewType() const;

  /// Get non-owning pointer to the camera
  camera::Camera* getCamera();

  /// Get non-owning pointers to the interaction handlers
  CameraInteractionHandler* getCameraHandler();
  CrosshairsInteractionHandler* getCrosshairsHandler();
  RefImageInteractionHandler* getRefImageHandler();
  SlideStackInteractionHandler* getStackHandler();
  SlideInteractionHandler* getSlideHandler();
  WindowLevelInteractionHandler* getWindowLevelHandler();

  /// Get non-owning pointer to the active interaction handler
  IInteractionHandler* getActiveHandler();

  /// Set the active interaction handler type
  void setActiveHandlerType(const InteractionHandlerType& type);

private:
  const gui::ViewType m_viewType;

  std::unique_ptr<camera::Camera> m_camera;

  std::unique_ptr<CameraInteractionHandler> m_cameraHandler;
  std::unique_ptr<CrosshairsInteractionHandler> m_crosshairsHandler;
  std::unique_ptr<RefImageInteractionHandler> m_refImageHandler;
  std::unique_ptr<SlideStackInteractionHandler> m_stackHandler;
  std::unique_ptr<SlideInteractionHandler> m_slideHandler;
  std::unique_ptr<WindowLevelInteractionHandler> m_windowLevelHandler;

  IInteractionHandler* m_activeHandler;
};

#endif // INTERACTION_PACK_H
