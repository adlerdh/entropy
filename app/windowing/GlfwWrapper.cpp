#include "windowing/GlfwWrapper.h"

#include "EntropyApp.h"
#include "common/Exception.hpp"
#include "common/Viewport.h"
#include "rendering/gl/OpenGLContext.h"
#include "ui/LinuxUiScale.h"
#include "ui/ImGuiWrapper.h"
#include "windowing/GlfwCallbacks.h"
#include "windowing/WindowData.h"

#ifdef _WIN32
#include "ui/menus/WinNativeMainMenu.h"
#endif

#include <spdlog/spdlog.h>

#include <glm/vec2.hpp>

#if defined(__linux__)
#define STBI_ONLY_PNG
#include <stb_image.h>

#include <cmrc/cmrc.hpp>
#endif

#include <glad/glad.h>
#include <imgui.h>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <algorithm>
#include <cmath>
#include <exception>
#include <iterator>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>

#if defined(__linux__)
CMRC_DECLARE(icons);
#endif

namespace
{
constexpr double kContentScalePollIntervalSeconds = 5.0;
constexpr float kScaleEpsilon = 0.001f;

#if defined(__linux__)
void setWindowIcon(GLFWwindow* window)
{
  cmrc::file icon;
  try {
    const auto filesystem = cmrc::icons::get_filesystem();
    icon = filesystem.open(ENTROPY_ABOUT_ICON_RESOURCE_PATH);
  }
  catch (const std::exception& e) {
    spdlog::warn("Unable to load embedded window icon: {}", e.what());
    return;
  }

  int width = 0;
  int height = 0;
  int channels = 0;
  stbi_uc* pixels = stbi_load_from_memory(
    reinterpret_cast<const stbi_uc*>(icon.begin()),
    static_cast<int>(icon.size()),
    &width,
    &height,
    &channels,
    4);

  if (!pixels || width <= 0 || height <= 0) {
    stbi_image_free(pixels);
    spdlog::warn("Unable to decode embedded window icon");
    return;
  }

  GLFWimage image{width, height, pixels};
  glfwSetWindowIcon(window, 1, &image);
  stbi_image_free(pixels);
}
#endif

// Ignore small differences in reported display scales.
bool nearlyEqual(float a, float b)
{
  return std::abs(a - b) < kScaleEpsilon;
}

glm::vec2 glfwWindowContentScale(GLFWwindow* window)
{
  float xscale = 1.0f;
  float yscale = 1.0f;
  glfwGetWindowContentScale(window, &xscale, &yscale);
  return {xscale, yscale};
}

glm::vec2 monitorContentScale(GLFWmonitor* monitor)
{
  if (!monitor) {
    return {1.0f, 1.0f};
  }

  float xscale = 1.0f;
  float yscale = 1.0f;
  glfwGetMonitorContentScale(monitor, &xscale, &yscale);
  return {xscale, yscale};
}

// Derive the scale from pixel and window sizes when both are valid.
std::optional<glm::vec2> framebufferContentScale(GLFWwindow* window)
{
  int windowWidth = 0;
  int windowHeight = 0;
  int fbWidth = 0;
  int fbHeight = 0;
  glfwGetWindowSize(window, &windowWidth, &windowHeight);
  glfwGetFramebufferSize(window, &fbWidth, &fbHeight);

  if (windowWidth <= 0 || windowHeight <= 0 || fbWidth <= 0 || fbHeight <= 0) {
    return std::nullopt;
  }

  return glm::vec2{
    static_cast<float>(fbWidth) / static_cast<float>(windowWidth),
    static_cast<float>(fbHeight) / static_cast<float>(windowHeight)};
}

// Combine platform scale reports for the Auto UI scale setting.
glm::vec2 resolvePolledContentScale(
  const glm::vec2& windowScale,
  const glm::vec2& monitorScale,
  const std::optional<glm::vec2>& framebufferScale,
  std::optional<float> desktopScale)
{
  glm::vec2 scale = windowScale;

  if (framebufferScale) {
#if defined(__linux__)
    scale.x = nearlyEqual(framebufferScale->x, monitorScale.x)
                ? framebufferScale->x
                : std::max({scale.x, framebufferScale->x, monitorScale.x});
    scale.y = nearlyEqual(framebufferScale->y, monitorScale.y)
                ? framebufferScale->y
                : std::max({scale.y, framebufferScale->y, monitorScale.y});
#else
    scale.x = std::max({scale.x, framebufferScale->x, monitorScale.x});
    scale.y = std::max({scale.y, framebufferScale->y, monitorScale.y});
#endif
  }
#if defined(__linux__)
  else {
    scale = monitorScale;
  }

  // Saved desktop configurations need not describe the monitor hosting this window.
  // Use them only when live window/monitor information is unavailable.
  scale.x = ui::linux_ui_scale::liveOrFallbackScale(scale.x, desktopScale);
  scale.y = ui::linux_ui_scale::liveOrFallbackScale(scale.y, desktopScale);
#else
  (void)desktopScale;
#endif

  return scale;
}
} // namespace

GlfwWrapper::GlfwWrapper(EntropyApp* app, int glMajorVersion, int glMinorVersion) : m_app(app)
{
  if (!app) {
    spdlog::critical("Cannot create the GLFW window without application state; Entropy cannot start");
    throwDebug("The application is null");
  }

  spdlog::debug("OpenGL Core profile version {}.{}", glMajorVersion, glMinorVersion);

  if (!glfwInit()) {
    spdlog::critical("Failed to initialize the GLFW windowing library; Entropy cannot start");
    throwDebug("Failed to initialize the GLFW windowing library");
  }

  spdlog::debug("Initialized GLFW windowing library");

  glfwSetErrorCallback(errorCallback);

  m_platform = glfwGetPlatform();
  if (m_platform == GLFW_PLATFORM_NULL) {
    spdlog::critical("GLFW reported that it is not initialized; Entropy cannot start");
    throwDebug("GLFW was not initialized");
  }

  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, glMajorVersion);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, glMinorVersion);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

  // Request an RGBA framebuffer with depth and stencil buffers.
  glfwWindowHint(GLFW_RED_BITS, 8);
  glfwWindowHint(GLFW_GREEN_BITS, 8);
  glfwWindowHint(GLFW_BLUE_BITS, 8);
  glfwWindowHint(GLFW_ALPHA_BITS, 8);
  glfwWindowHint(GLFW_DEPTH_BITS, 24);
  glfwWindowHint(GLFW_STENCIL_BITS, 8);

  glfwWindowHint(GLFW_SAMPLES, 4);

  glfwWindowHint(GLFW_DOUBLEBUFFER, GLFW_TRUE);
  glfwWindowHint(GLFW_MAXIMIZED, GLFW_TRUE);
  glfwWindowHint(GLFW_VISIBLE, GLFW_TRUE);

  glfwWindowHint(GLFW_FOCUS_ON_SHOW, GLFW_TRUE);

  // Request window resizing to follow monitor scaling where supported.
  glfwWindowHint(GLFW_SCALE_TO_MONITOR, GLFW_TRUE);

#if defined(__linux__)
  glfwWindowHintString(GLFW_X11_CLASS_NAME, "io.github.adlerdh.entropy");
  glfwWindowHintString(GLFW_X11_INSTANCE_NAME, "io.github.adlerdh.entropy");
  glfwWindowHintString(GLFW_WAYLAND_APP_ID, "io.github.adlerdh.entropy");
#endif

#ifdef __APPLE__
  // macOS requires a forward-compatible context for modern OpenGL.
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);

  // Render at full pixel resolution on Retina displays.
  glfwWindowHint(GLFW_COCOA_RETINA_FRAMEBUFFER, GLFW_TRUE);

  // Disable automatic GPU switching on Macs that support it.
  glfwWindowHint(GLFW_COCOA_GRAPHICS_SWITCHING, GLFW_FALSE);

  glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER, GLFW_FALSE);

  // Use a stable name for macOS window-frame autosaving.
  glfwWindowHintString(GLFW_COCOA_FRAME_NAME, "EntropyViewer");

  spdlog::debug("Initialized GLFW window and context for Apple macOS platform");
#endif

  // Prefer the primary monitor's work area for the initial window size.
  int width = static_cast<int>(app->windowData().viewport().width());
  int height = static_cast<int>(app->windowData().viewport().height());

  if (GLFWmonitor* monitor = glfwGetPrimaryMonitor()) {
    int xpos = 0, ypos = 0;
    glfwGetMonitorWorkarea(monitor, &xpos, &ypos, &width, &height);
  }

  m_window = glfwCreateWindow(width, height, "Entropy", nullptr, nullptr);

#ifndef __APPLE__
  if (!m_window) {
    spdlog::info(
      "Failed to create requested OpenGL {}.{} Core context with multisampling; retrying without multisampling",
      glMajorVersion,
      glMinorVersion);
    glfwWindowHint(GLFW_SAMPLES, 0);
    m_window = glfwCreateWindow(width, height, "Entropy", nullptr, nullptr);
  }

  if (!m_window) {
    spdlog::warn(
      "Failed to create requested OpenGL {}.{} Core context; retrying without a core profile hint",
      glMajorVersion,
      glMinorVersion);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_ANY_PROFILE);
    glfwWindowHint(GLFW_SAMPLES, 0);
    m_window = glfwCreateWindow(width, height, "Entropy", nullptr, nullptr);
  }
#endif

  if (!m_window) {
    glfwTerminate();
    spdlog::critical(
      "Could not create an OpenGL {}.{} window or context after all compatibility attempts; Entropy cannot start",
      glMajorVersion,
      glMinorVersion);
    throwDebug("Failed to create GLFW window and context");
  }

  spdlog::debug("Created GLFW window and context");

#if defined(__linux__)
  setWindowIcon(m_window);
#endif

  // Let GLFW callbacks access the application through the window.
  glfwSetWindowUserPointer(m_window, reinterpret_cast<void*>(app));

  glfwMakeContextCurrent(m_window);

  // Request VSync to reduce tearing; actual swap timing depends on the platform.
  glfwSwapInterval(1);

  // Register window and input callbacks.
  glfwSetWindowContentScaleCallback(m_window, windowContentScaleCallback);
  glfwSetWindowCloseCallback(m_window, windowCloseCallback);
  glfwSetWindowFocusCallback(m_window, windowFocusCallback);
  glfwSetWindowPosCallback(m_window, windowPositionCallback); // not called on Wayland
  glfwSetWindowSizeCallback(m_window, windowSizeCallback);
  glfwSetFramebufferSizeCallback(m_window, framebufferSizeCallback);

  glfwSetCursorPosCallback(m_window, cursorPosCallback);
  glfwSetDropCallback(m_window, dropCallback);
  glfwSetKeyCallback(m_window, keyCallback);
  glfwSetMouseButtonCallback(m_window, mouseButtonCallback);
  glfwSetScrollCallback(m_window, scrollCallback);

  glfwSetWindowAttrib(m_window, GLFW_DECORATED, GLFW_TRUE);

  spdlog::debug("Set GLFW callbacks");

  // Create cursors: not currently used
  GLFWcursor* windowLevelCursor = glfwCreateStandardCursor(GLFW_IBEAM_CURSOR);
  m_mouseModeToCursor.emplace(MouseMode::WindowLevel, windowLevelCursor);

  spdlog::debug("Created GLFW cursors");

  // Load all OpenGL function pointers with GLAD
  if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress))) {
    glfwDestroyWindow(m_window);
    m_window = nullptr;
    glfwTerminate();
    spdlog::critical("Failed to load OpenGL function pointers with GLAD; Entropy cannot start");
    throwDebug("Failed to load OpenGL function pointers with GLAD");
  }

  spdlog::debug("Loaded OpenGL function pointers with GLAD");
  try {
    validateOpenGLContext();
  }
  catch (...) {
    glfwDestroyWindow(m_window);
    m_window = nullptr;
    glfwTerminate();
    throw;
  }
}

GlfwWrapper::~GlfwWrapper()
{
  for (auto& cursorLocal : m_mouseModeToCursor) {
    if (cursorLocal.second) {
      glfwDestroyCursor(cursorLocal.second);
    }
  }

#ifdef _WIN32
  uninstallWindowsNativeMainMenu(m_window);
#endif

  glfwDestroyWindow(m_window);
  glfwTerminate();
  spdlog::debug("Destroyed window and terminated GLFW");
}

void GlfwWrapper::setCallbacks(
  std::function<void(std::chrono::time_point<std::chrono::steady_clock>& lastFrameTime)> framerateLimiter,
  std::function<void()> renderScene,
  std::function<void()> renderGui,
  std::function<void()> processBackground)
{
  m_framerateLimiter = std::move(framerateLimiter);
  m_renderScene = std::move(renderScene);
  m_renderGui = std::move(renderGui);
  m_processBackground = std::move(processBackground);
}

void GlfwWrapper::setEventProcessingMode(EventProcessingMode mode)
{
  m_eventProcessingMode = mode;
}

void GlfwWrapper::setWaitTimeout(double waitTimoutSeconds)
{
  m_waitTimoutSeconds = waitTimoutSeconds;
}

void GlfwWrapper::init()
{
  if (m_platform != GLFW_PLATFORM_WAYLAND && m_platform != GLFW_PLATFORM_NULL) {
    glfwGetWindowPos(m_window, &m_backupWindowPosX, &m_backupWindowPosY);
  }

  windowPositionCallback(m_window, m_backupWindowPosX, m_backupWindowPosY);

  glfwGetWindowSize(m_window, &m_backupWindowWidth, &m_backupWindowHeight);
  m_backupMaximized = (glfwGetWindowAttrib(m_window, GLFW_MAXIMIZED) == GLFW_TRUE);

  windowSizeCallback(m_window, m_backupWindowWidth, m_backupWindowHeight);

  int fbWidth = 0;
  int fbHeight = 0;
  glfwGetFramebufferSize(m_window, &fbWidth, &fbHeight);
  framebufferSizeCallback(m_window, fbWidth, fbHeight);

  float xscale = std::numeric_limits<float>::quiet_NaN();
  float yscale = std::numeric_limits<float>::quiet_NaN();
  glfwGetWindowContentScale(m_window, &xscale, &yscale);
  windowContentScaleCallback(m_window, xscale, yscale);

  glfwShowWindow(m_window);

  spdlog::debug("Initialized GLFW wrapper");
}

void GlfwWrapper::renderLoop(
  std::atomic<bool>& imagesReady,
  const std::atomic<bool>& imageLoadFailed,
  const std::function<bool(void)>& checkAppQuit,
  const std::function<void(void)>& onImagesReady)
{
  using Clock = std::chrono::steady_clock;
  constexpr bool logFramerate = false;

  if (!m_renderScene || !m_renderGui) {
    spdlog::critical("Rendering callbacks were not initialized; Entropy cannot continue");
    throwDebug("Rendering callbacks not initialized");
  }

  spdlog::debug("Starting GLFW rendering loop");

  auto lastFrameTime = Clock::now();

  while (!glfwWindowShouldClose(m_window)) {
    if (checkAppQuit()) {
      spdlog::info("User has quit the application");
      break;
    }

    if (imagesReady) {
      imagesReady = false;
      onImagesReady();
      init(); // Call initial windowing callbacks one more time
    }

    if (imageLoadFailed) {
      throw std::runtime_error("Render loop exiting due to failure to load images");
    }

    if (m_framerateLimiter) {
      m_framerateLimiter(lastFrameTime);
    }

    if (m_processBackground) {
      m_processBackground();
    }

    processInput();
    renderOnce();

    glfwSwapBuffers(m_window);

    switch (m_eventProcessingMode) {
      case EventProcessingMode::Poll: {
        glfwPollEvents();
        break;
      }
      case EventProcessingMode::Wait: {
        glfwWaitEventsTimeout(1.0);
        break;
      }
      case EventProcessingMode::WaitTimeout: {
        glfwWaitEventsTimeout(m_waitTimoutSeconds);
        break;
      }
    }

    if (logFramerate) {
      SPDLOG_TRACE("Frame rate: {}", ImGui::GetIO().Framerate);
    }
  }

  spdlog::debug("Done GLFW rendering loop");
}

void GlfwWrapper::renderOnce()
{
  syncWindowAndFramebufferSizes();
  syncContentScale();
  m_renderScene();
  m_renderGui();
}

void GlfwWrapper::renderAndSwapOnce()
{
  if (!m_renderScene || !m_renderGui) {
    return;
  }

  renderOnce();
  glfwSwapBuffers(m_window);
}

void GlfwWrapper::postEmptyEvent()
{
  glfwPostEmptyEvent();
}

void GlfwWrapper::processInput()
{
  // No inputs are currently being processed here
}

void GlfwWrapper::syncWindowAndFramebufferSizes()
{
  if (!m_app || !m_window) {
    return;
  }

  int windowWidth = 0;
  int windowHeight = 0;
  int fbWidth = 0;
  int fbHeight = 0;
  glfwGetWindowSize(m_window, &windowWidth, &windowHeight);
  glfwGetFramebufferSize(m_window, &fbWidth, &fbHeight);

  m_app->windowData().setFramebufferSize(fbWidth, fbHeight);
  m_app->resize(windowWidth, windowHeight);
}

void GlfwWrapper::syncContentScale()
{
  if (!m_app || !m_window) {
    return;
  }

  // Apply callbacks immediately, and poll as a fallback for missed scale changes.
  const double now = glfwGetTime();
  if (m_lastContentScalePollSeconds >= 0.0 && now - m_lastContentScalePollSeconds < kContentScalePollIntervalSeconds) {
    return;
  }
  m_lastContentScalePollSeconds = now;

  std::optional<float> desktopScale;
#if defined(__linux__)
  // Use GNOME's saved scale only if the live scale reports are invalid.
  desktopScale = ui::linux_ui_scale::primaryMonitorScale();
#endif

  const glm::vec2 scale = resolvePolledContentScale(
    glfwWindowContentScale(m_window),
    monitorContentScale(currentMonitor()),
    framebufferContentScale(m_window),
    desktopScale);

  const glm::vec2 currentScale = m_app->windowData().getContentScaleRatios();
  if (nearlyEqual(currentScale.x, scale.x) && nearlyEqual(currentScale.y, scale.y)) {
    return;
  }

  spdlog::debug(
    "Polled platform content scale change: {}x{} -> {}x{}",
    currentScale.x,
    currentScale.y,
    scale.x,
    scale.y);

  m_app->windowData().setContentScaleRatios(scale);
  m_app->imgui().setContentScale(m_app->windowData().getContentScaleRatio());
}

const GLFWwindow* GlfwWrapper::window() const
{
  return m_window;
}

GLFWwindow* GlfwWrapper::window()
{
  return m_window;
}

GLFWcursor* GlfwWrapper::cursor(MouseMode mode)
{
  const auto it = m_mouseModeToCursor.find(mode);
  if (std::end(m_mouseModeToCursor) != it) {
    return it->second;
  }
  return nullptr;
}

void GlfwWrapper::setWindowTitleStatus(const std::string& status)
{
  static const std::string s_entropy("Entropy");

  if (status.empty()) {
    glfwSetWindowTitle(m_window, s_entropy.c_str());
  }
  else {
    const std::string statusString(s_entropy + std::string(" [") + status + std::string("]"));
    glfwSetWindowTitle(m_window, statusString.c_str());
  }
}

void GlfwWrapper::toggleFullScreenMode(bool forceWindowMode)
{
  const bool isFullScreen = (nullptr != glfwGetWindowMonitor(m_window));

  if (forceWindowMode || isFullScreen) {
    // Restore windowed mode with backup of position and size:
    glfwSetWindowMonitor(
      m_window,
      nullptr,
      m_backupWindowPosX,
      m_backupWindowPosY,
      m_backupWindowWidth,
      m_backupWindowHeight,
      GLFW_DONT_CARE);
  }
  else {
    // Switch to full-screen mode after backing up position and size:
    if (m_platform != GLFW_PLATFORM_WAYLAND && m_platform != GLFW_PLATFORM_NULL) {
      glfwGetWindowPos(m_window, &m_backupWindowPosX, &m_backupWindowPosY);
    }

    glfwGetWindowSize(m_window, &m_backupWindowWidth, &m_backupWindowHeight);
    m_backupMaximized = (glfwGetWindowAttrib(m_window, GLFW_MAXIMIZED) == GLFW_TRUE);

    GLFWmonitor* monitor = currentMonitor();
    if (!monitor) {
      spdlog::error("Null monitor upon setting full-screen mode.");
      return;
    }

    const GLFWvidmode* mode = glfwGetVideoMode(monitor);
    if (!mode) {
      spdlog::error("Null video mode upon setting full-screen mode.");
      return;
    }

    /// @todo Check GLFW_PLATFORM_ERROR after this call
    glfwSetWindowMonitor(m_window, monitor, 0, 0, mode->width, mode->height, GLFW_DONT_CARE);
  }
}

GLFWmonitor* GlfwWrapper::currentMonitor() const
{
  // The current monitor is the one with the largest overlap with the window.
  // Initialize to the primary monitor.
  GLFWmonitor* currentMonitor = glfwGetPrimaryMonitor();
  int largestOverlap = 0;

  int winPosX = 0;
  int winPosY = 0;
  int winWidth = 0;
  int winHeight = 0;

  if (m_platform != GLFW_PLATFORM_WAYLAND && m_platform != GLFW_PLATFORM_NULL) {
    glfwGetWindowPos(m_window, &winPosX, &winPosY);
  }

  glfwGetWindowSize(m_window, &winWidth, &winHeight);

  int numMonitors = 0;
  GLFWmonitor** monitors = glfwGetMonitors(&numMonitors);

  for (int i = 0; i < numMonitors; ++i) {
    if (!monitors[i]) {
      spdlog::debug("Monitor {} is null", i);
      continue;
    }

    const GLFWvidmode* mode = glfwGetVideoMode(monitors[i]);
    if (!mode) {
      spdlog::debug("Video mode for monitor {} is null", i);
      continue;
    }

    int monitorPosX = 0, monitorPosY = 0;
    glfwGetMonitorPos(monitors[i], &monitorPosX, &monitorPosY);

    const int monitorWidth = mode->width;
    const int monitorHeight = mode->height;

    const int overlapX =
      std::max(std::min(winPosX + winWidth, monitorPosX + monitorWidth) - std::max(winPosX, monitorPosX), 0);

    const int overlapY =
      std::max(std::min(winPosY + winHeight, monitorPosY + monitorHeight) - std::max(winPosY, monitorPosY), 0);

    const int overlap = overlapX * overlapY;

    if (largestOverlap < overlap) {
      largestOverlap = overlap;
      currentMonitor = monitors[i];
    }
  }

  return currentMonitor;
}
