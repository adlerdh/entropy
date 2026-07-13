#pragma once

#include "common/Types.h"

#include <atomic>
#include <chrono>
#include <functional>
#include <string>
#include <unordered_map>

class EntropyApp;

struct GLFWcursor;
struct GLFWmonitor;
struct GLFWwindow;

/** @brief GLFW event wait mode used by the render loop. */
enum class EventProcessingMode
{
  Poll,       //!< Process queued events and return
  Wait,       //!< Wait until an event arrives
  WaitTimeout //!< Wait until an event arrives or the timeout expires
};

/** @brief Owns the GLFW window, cursors, callbacks, and render loop. */
class GlfwWrapper
{
public:
  /**
   * @brief Construct the GLFW wrapper.
   * @param app Application stored as the GLFW window user pointer.
   * @param glMajorVersion Requested OpenGL major version.
   * @param glMinorVersion Requested OpenGL minor version.
   */
  GlfwWrapper(EntropyApp* app, int glMajorVersion, int glMinorVersion);
  ~GlfwWrapper();

  /** @brief Install callbacks invoked by render calls. */
  void setCallbacks(
    std::function<void(std::chrono::time_point<std::chrono::steady_clock>& lastFrameTime)> framerateLimiter,
    std::function<void()> renderScene,
    std::function<void()> renderGui,
    std::function<void()> processBackground);

  /** @brief Set the event processing mode for the render loop. */
  void setEventProcessingMode(EventProcessingMode mode);

  /** @brief Set the wait timeout used by `EventProcessingMode::WaitTimeout`. */
  void setWaitTimeout(double waitTimoutSeconds);

  /**
   * @brief Initialize the GLFW window.
   * @note Requires rendering to be initialized, since it kicks off a frame render in the
   * framebufferSizeCallback
   */
  void init();

  /**
   * @brief Execute the render loop.
   * @param imagesReady Set when images are ready; cleared after `onImagesReady`.
   * @param imageLoadFailed Set when background image loading failed.
   * @param checkAppQuit Returns true when the application should quit.
   * @param onImagesReady Called once when `imagesReady` is set.
   */
  void renderLoop(
    std::atomic<bool>& imagesReady,
    const std::atomic<bool>& imageLoadFailed,
    const std::function<bool(void)>& checkAppQuit,
    const std::function<void(void)>& onImagesReady);

  /** @brief Render one frame without forcing a buffer swap. */
  void renderOnce();

  /** @brief Render one frame and swap buffers, if render callbacks are initialized. */
  void renderAndSwapOnce();

  /**
   * @brief Post an empty event from the current thread to the GLFW event queue,
   * causing glfwWaitEvents() to return.
   * @note This may be called from any thread.
   */
  void postEmptyEvent();

  const GLFWwindow* window() const;
  GLFWwindow* window();

  /** @brief Get the cursor for a mouse mode. */
  GLFWcursor* cursor(MouseMode mode);

  void setWindowTitleStatus(const std::string& status);

  void toggleFullScreenMode(bool forceWindowMode = false);

private:
  // Synchronize Entropy's cached window and framebuffer sizes with GLFW.
  void syncWindowAndFramebufferSizes();

  /**
   * @brief Refresh Entropy's cached platform UI scale when callbacks are stale.
   *
   * GLFW content-scale callbacks are still the primary immediate path. This
   * throttled fallback handles Linux desktop scale changes that update GNOME
   * display configuration without updating GLFW's window scale metadata.
   */
  void syncContentScale();

  // Process user interaction input between render calls.
  void processInput();

  // Returns the "current monitor" of the window. This is evaluated
  // as the monitor with the largest overlap with the window.
  GLFWmonitor* currentMonitor() const;

  double m_lastContentScalePollSeconds = -1.0; //!< Last fallback content-scale poll time
  int m_platform;                              //!< GLFW platform selected during initialization

  // Application owning this wrapper. GLFW also stores this as the window user pointer.
  EntropyApp* m_app = nullptr;

  // GLFW window that is owned by this class:
  GLFWwindow* m_window;

  // Map from mouse mode to cursor
  std::unordered_map<MouseMode, GLFWcursor*> m_mouseModeToCursor;

  // Allows this class to change how window events are processed:
  EventProcessingMode m_eventProcessingMode = EventProcessingMode::Wait;

  // For EventProcessingMode::WaitTimeout, this is the timeout in seconds:
  double m_waitTimoutSeconds = 1.0 / 3.0;

  // Rendering callbacks:
  std::function<void(std::chrono::time_point<std::chrono::steady_clock>& lastFrameTime)> m_framerateLimiter = nullptr;
  std::function<void()> m_renderScene = nullptr;
  std::function<void()> m_renderGui = nullptr;
  std::function<void()> m_processBackground = nullptr;

  // Backups of window position and size, which are restored when changing from full-screen to
  // windowed mode
  int m_backupWindowPosX = 0;
  int m_backupWindowPosY = 0;

  int m_backupWindowWidth = 1;
  int m_backupWindowHeight = 1;

  bool m_backupMaximized = true;
};
