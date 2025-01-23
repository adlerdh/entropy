#ifndef GLFW_WRAPPER_H
#define GLFW_WRAPPER_H

#include "common/Types.h"

#include <atomic>
#include <functional>
#include <unordered_map>

class EntropyApp;

struct GLFWcursor;
struct GLFWmonitor;
struct GLFWwindow;

/**
 * @brief Describes the event processing mode in the GLFW render loop
 */
enum class EventProcessingMode
{
  /// Process only those events that are already in the event queue and then returns immediately.
  /// Processing events will cause the window and input callbacks associated with those events to
  /// be called.
  Poll,

  /// Puts the calling thread to sleep until at least one event is available in the event queue.
  /// Once one or more events are available, events in the queue are processed and the function
  /// then returns immediately (just like Poll). Processing events will cause the window and input
  /// callbacks associated with those events to be called.
  Wait,

  /// Puts the calling thread to sleep until at least one event is available in the event queue,
  /// or until the specified timeout is reached. If one or more events are available, it behaves
  /// exactly like Poll, i.e. the events in the queue are processed and the function then returns
  /// immediately. Processing events will cause the window and input callbacks associated with
  /// those events to be called.
  WaitTimeout
};

/**
 * @brief A simple wrapper for GLFW windowing. This class owns the GLFW window.
 */
class GlfwWrapper
{
public:
  /**
     * @brief Construct the GLFW wrapper
     * @param app Pointer to the app that will be embedded in the GLFW window
     * @param glMajorVersion
     * @param glMinorVersion
     */
  GlfwWrapper(EntropyApp* app, int glMajorVersion, int glMinorVersion);
  ~GlfwWrapper();

  /**
     * @brief setCallbacks
     * @param renderScene
     * @param renderGui
     */
  void setCallbacks(std::function<void()> renderScene, std::function<void()> renderGui);

  /// @brief Set the event processing mode for the render loop
  void setEventProcessingMode(EventProcessingMode mode);

  /// @brief Set the wait timeout in seconds. This timeout only applies when the event processing
  /// mode is set to EventProcessingMode::WaitTimeout.
  void setWaitTimeout(double waitTimoutSeconds);

  /**
     * @brief init
     * @note Requires rendering to be initialized, since it kicks off a frame render in the
     * framebufferSizeCallback
     */
  void init();

  /**
     * @brief Execute the render loop
     *
     * @param[in,out] imagesReady True iff images have been loaded into memory.
     * Set to false after onImagesReady is called.
     * @param[in] checkAppQuit Function to check if the application should quit
     * @param[in] onImagesReady Function to call when images are ready
     */
  void renderLoop(
    std::atomic<bool>& imagesReady,
    const std::atomic<bool>& imageLoadFailed,
    const std::function<bool(void)>& checkAppQuit,
    const std::function<void(void)>& onImagesReady
  );

  /**
     * @brief Render one frame
     */
  void renderOnce();

  /**
     * @brief Post an empty event from the current thread to the GLFW event queue,
     * causing glfwWaitEvents() to return.
     * @note This may be called from any thread.
     */
  void postEmptyEvent();

  const GLFWwindow* window() const;
  GLFWwindow* window();

  /// @brief Get the cursor for a mouse mode
  GLFWcursor* cursor(MouseMode mode);

  void setWindowTitleStatus(const std::string& status);

  void toggleFullScreenMode(bool forceWindowMode = false);

private:
  // Process user interaction input between render calls.
  void processInput();

  // Returns the "current monitor" of the window. This is evaluated
  // as the monitor with the largest overlap with the window.
  GLFWmonitor* currentMonitor() const;

  // GLFW window that is owned by this class:
  GLFWwindow* m_window;

  // Map from mouse mode to cursor
  std::unordered_map<MouseMode, GLFWcursor*> m_mouseModeToCursor;

  // Allows this class to change how window events are processed:
  EventProcessingMode m_eventProcessingMode;

  // For EventProcessingMode::WaitTimeout, this is the timeout in seconds:
  double m_waitTimoutSeconds;

  // Rendering callbacks:
  std::function<void()> m_renderScene;
  std::function<void()> m_renderGui;

  // Backups of window position and size, which are restored when changing
  // from full-screen to windowed mode
  int m_backupWindowPosX;
  int m_backupWindowPosY;

  int m_backupWindowWidth;
  int m_backupWindowHeight;
};

#endif // GLFW_WRAPPER_H
