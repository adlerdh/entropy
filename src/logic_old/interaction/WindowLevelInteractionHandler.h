#ifndef WINDOW_LEVEL_INTERACTION_HANDLER_H
#define WINDOW_LEVEL_INTERACTION_HANDLER_H

#include "logic/interaction/InteractionHandlerBase.h"
#include "logic/interaction/InteractionModes.h"

#include <glm/vec2.hpp>

#include <functional>
#include <memory>
#include <unordered_map>

namespace imageio
{
class ImageCpuRecord;
}

/**
 * @brief Handle interactive changes to image intensity window and level settings.
 */
class WindowLevelInteractionHandler : public InteractionHandlerBase
{
  /// Function providing const access to the active image CPU record
  using ActiveImageCpuRecordRequesterType = std::function<const imageio::ImageCpuRecord*(void)>;

  /// Function for broadcasting that the active image's window and level have changed
  using ActiveImageWindowLevelBroadcasterType = std::function<void(double window, double level)>;

public:
  explicit WindowLevelInteractionHandler();
  ~WindowLevelInteractionHandler() override = default;

  void setActiveImageCpuRecordRequester(ActiveImageCpuRecordRequesterType);
  void setActiveImageWindowLevelBroadcaster(ActiveImageWindowLevelBroadcasterType);

  void setMode(const WindowLevelInteractionMode&);

private:
  enum class MouseMoveMode
  {
    WindowAndLevel,
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

  bool changeWindowLevel(
    const glm::vec2& ndcOldPos, const glm::vec2& ndcNewPos, double scaleFactor = 1.0
  );
  bool changeWindow(const glm::vec2& ndcOldPos, const glm::vec2& ndcNewPos, double scaleFactor = 1.0);
  bool changeLevel(const glm::vec2& ndcOldPos, const glm::vec2& ndcNewPos, double scaleFactor = 1.0);

  bool changeWindow(double delta, double scaleFactor = 1.0);
  bool changeLevel(double delta, double scaleFactor = 1.0);

  ActiveImageCpuRecordRequesterType m_activeImageRequester;
  ActiveImageWindowLevelBroadcasterType m_activeImageWindowLevelBroadcaster;

  WindowLevelInteractionMode m_primaryMode;
  MouseMoveMode m_mouseMoveMode;

  glm::vec2 m_ndcLeftButtonStartPos;
  glm::vec2 m_ndcRightButtonStartPos;
  glm::vec2 m_ndcMiddleButtonStartPos;

  glm::vec2 m_ndcLeftButtonLastPos;
  glm::vec2 m_ndcRightButtonLastPos;
  glm::vec2 m_ndcMiddleButtonLastPos;
};

#endif // WINDOW_LEVEL_INTERACTION_HANDLER_H
