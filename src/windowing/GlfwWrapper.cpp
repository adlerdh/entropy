#include "windowing/GlfwWrapper.h"

#include "EntropyApp.h"
#include "common/Exception.hpp"
#include "windowing/GlfwCallbacks.h"

#include <spdlog/spdlog.h>

#include <glad/glad.h>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

namespace
{
static const std::string s_entropy("Entropy");
}

GlfwWrapper::GlfwWrapper(EntropyApp* app, int glMajorVersion, int glMinorVersion)
  : m_eventProcessingMode(EventProcessingMode::Wait)
  , m_waitTimoutSeconds(1.0 / 30.0)
  ,

  m_renderScene(nullptr)
  , m_renderGui(nullptr)
  ,

  m_backupWindowPosX(0)
  , m_backupWindowPosY(0)
  , m_backupWindowWidth(1)
  , m_backupWindowHeight(1)
{
  if (!app)
  {
    spdlog::critical("The application is null on GLFW creation");
    throw_debug("The application is null")
  }

  spdlog::debug("OpenGL Core profile version {}.{}", glMajorVersion, glMinorVersion);

  if (!glfwInit())
  {
    spdlog::critical("Failed to initialize the GLFW windowing library");
    throw_debug("Failed to initialize the GLFW windowing library")
  }

  spdlog::debug("Initialized GLFW windowing library");

  // Set OpenGL version
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, glMajorVersion);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, glMinorVersion);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

  // Desired bit depths of the components of the window's default framebuffer
  glfwWindowHint(GLFW_RED_BITS, 8);
  glfwWindowHint(GLFW_GREEN_BITS, 8);
  glfwWindowHint(GLFW_BLUE_BITS, 8);
  glfwWindowHint(GLFW_ALPHA_BITS, 8);
  glfwWindowHint(GLFW_DEPTH_BITS, 24);
  glfwWindowHint(GLFW_STENCIL_BITS, 8);

  // Desired number of samples to use for multisampling
  glfwWindowHint(GLFW_SAMPLES, 4);

  glfwWindowHint(GLFW_DOUBLEBUFFER, GLFW_TRUE);
  glfwWindowHint(GLFW_MAXIMIZED, GLFW_TRUE);

  glfwWindowHint(GLFW_VISIBLE, GLFW_TRUE);

  // Window will be given input focus when glfwShowWindow is called
  glfwWindowHint(GLFW_FOCUS_ON_SHOW, GLFW_TRUE);

  // Window content area should be resized based on the monitor content scale of any
  // monitor it is placed on. This includes the initial placement when the window is created
  glfwWindowHint(GLFW_SCALE_TO_MONITOR, GLFW_TRUE);

#ifdef __APPLE__
  // Window's context is an OpenGL forward-compatible, i.e. one where all functionality deprecated
  // in the requested version of OpenGL is removed (required on macOS)
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);

  // Use full resolution framebuffers on Retina displays
  glfwWindowHint(GLFW_COCOA_RETINA_FRAMEBUFFER, GLFW_FALSE);

  // Disable Automatic Graphics Switching, i.e. do not allow the system to choose the integrated GPU
  // for the OpenGL context and move it between GPUs if necessary. Forces it to always run on the discrete GPU.
  glfwWindowHint(GLFW_COCOA_GRAPHICS_SWITCHING, GLFW_FALSE);

  glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER, GLFW_FALSE);

  // Encoded name to use for autosaving the window frame
  glfwWindowHintString(GLFW_COCOA_FRAME_NAME, "EntropyViewer");

  spdlog::debug("Initialized GLFW window and context for Apple macOS platform");
#endif

  // Get window dimensions, using monitor work area if available
  int width = static_cast<int>(app->windowData().viewport().width());
  int height = static_cast<int>(app->windowData().viewport().height());

  if (GLFWmonitor* monitor = glfwGetPrimaryMonitor())
  {
    int xpos, ypos;
    glfwGetMonitorWorkarea(monitor, &xpos, &ypos, &width, &height);
  }

  m_window = glfwCreateWindow(width, height, "Entropy", nullptr, nullptr);

  if (!m_window)
  {
    glfwTerminate();
    throw_debug("Failed to create GLFW window and context")
  }

  spdlog::debug("Created GLFW window and context");

  // Embed pointer to application data in GLFW window
  glfwSetWindowUserPointer(m_window, reinterpret_cast<void*>(app));

  // Make window's context current on this thread
  glfwMakeContextCurrent(m_window);

  // Set callbacks:
  glfwSetErrorCallback(errorCallback);

  glfwSetWindowContentScaleCallback(m_window, windowContentScaleCallback);
  glfwSetWindowCloseCallback(m_window, windowCloseCallback);
  glfwSetWindowPosCallback(m_window, windowPositionCallback);
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
  GLFWcursor* cursor = glfwCreateStandardCursor(GLFW_IBEAM_CURSOR);
  m_mouseModeToCursor.emplace(MouseMode::WindowLevel, cursor);

  spdlog::debug("Created GLFW cursors");

  //    GLFWimage images[1];
  //    images[0].pixels = stbi_load("PATH", &images[0].width, &images[0].height, 0, 4); //rgba channels
  //    glfwSetWindowIcon(window, 1, images);
  //    stbi_image_free(images[0].pixels);

  //    GLFWimage icons[1];
  //    icons[0].pixels = SOIL_load_image("icon.png", &icons[0].width, &icons[0].height, 0, SOIL_LOAD_RGBA);
  //    glfwSetWindowIcon(window.window, 1, icons);
  //    SOIL_free_image_data(icons[0].pixels);

  // Load all OpenGL function pointers with GLAD
  if (!gladLoadGLLoader((GLADloadproc) glfwGetProcAddress))
  {
    glfwTerminate();
    spdlog::critical("Failed to load OpenGL function pointers with GLAD");
    throw_debug("Failed to load OpenGL function pointers with GLAD")
  }

  spdlog::debug("Loaded OpenGL function pointers with GLAD");
}

GlfwWrapper::~GlfwWrapper()
{
  for (auto& cursor : m_mouseModeToCursor)
  {
    if (cursor.second)
      glfwDestroyCursor(cursor.second);
  }

  glfwDestroyWindow(m_window);
  glfwTerminate();
  spdlog::debug("Destroyed window and terminated GLFW");
}

void GlfwWrapper::setCallbacks(std::function<void()> renderScene, std::function<void()> renderGui)
{
  m_renderScene = std::move(renderScene);
  m_renderGui = std::move(renderGui);
}

void GlfwWrapper::setEventProcessingMode(EventProcessingMode mode)
{
  m_eventProcessingMode = std::move(mode);
}

void GlfwWrapper::setWaitTimeout(double waitTimoutSeconds)
{
  m_waitTimoutSeconds = waitTimoutSeconds;
}

void GlfwWrapper::init()
{
  glfwGetWindowPos(m_window, &m_backupWindowPosX, &m_backupWindowPosY);
  windowPositionCallback(m_window, m_backupWindowPosX, m_backupWindowPosY);

  glfwGetWindowSize(m_window, &m_backupWindowWidth, &m_backupWindowHeight);
  windowSizeCallback(m_window, m_backupWindowWidth, m_backupWindowHeight);

  int fbWidth = 0;
  int fbHeight = 0;
  glfwGetFramebufferSize(m_window, &fbWidth, &fbHeight);
  framebufferSizeCallback(m_window, fbWidth, fbHeight);

  float xscale, yscale;
  glfwGetWindowContentScale(m_window, &xscale, &yscale);
  windowContentScaleCallback(m_window, xscale, yscale);

  glfwShowWindow(m_window);

  spdlog::debug("Initialized GLFW wrapper");
}

void GlfwWrapper::renderLoop(
  std::atomic<bool>& imagesReady,
  const std::atomic<bool>& imageLoadFailed,
  const std::function<bool(void)>& checkAppQuit,
  const std::function<void(void)>& onImagesReady
)
{
  if (!m_renderScene || !m_renderGui)
  {
    spdlog::critical("Rendering callbacks not initialized");
    throw_debug("Rendering callbacks not initialized")
  }

  spdlog::debug("Starting GLFW rendering loop");

  while (!glfwWindowShouldClose(m_window))
  {
    if (checkAppQuit())
    {
      spdlog::info("User has quit the application");
      break;
    }

    if (imagesReady)
    {
      imagesReady = false;
      onImagesReady();
      init(); // Call initial windowing callbacks one more time
    }

    if (imageLoadFailed)
    {
      spdlog::critical("Render loop exiting due to failure to load images");
      exit(EXIT_FAILURE);
    }

    processInput();
    renderOnce();

    glfwSwapBuffers(m_window);

    switch (m_eventProcessingMode)
    {
    case EventProcessingMode::Poll:
    {
      glfwPollEvents();
      break;
    }
    case EventProcessingMode::Wait:
    {
      glfwWaitEvents();
      break;
    }
    case EventProcessingMode::WaitTimeout:
    {
      glfwWaitEventsTimeout(m_waitTimoutSeconds);
      break;
    }
    }
  }

  spdlog::debug("Done GLFW rendering loop");
}

void GlfwWrapper::renderOnce()
{
  m_renderScene();
  m_renderGui();
}

void GlfwWrapper::postEmptyEvent()
{
  glfwPostEmptyEvent();
}

void GlfwWrapper::processInput()
{
  // No inputs are currently being processed here.

  // Could check inputs and react as shown below:
  //    if ( GLFW_PRESS == glfwGetKey( m_window, GLFW_KEY_ESCAPE ) )
  //    {
  //        glfwSetWindowShouldClose( m_window, true );
  //    }
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
  if (std::end(m_mouseModeToCursor) != it)
  {
    return it->second;
  }
  return nullptr;
}

void GlfwWrapper::setWindowTitleStatus(const std::string& status)
{
  if (status.empty())
  {
    glfwSetWindowTitle(m_window, s_entropy.c_str());
  }
  else
  {
    const std::string statusString(s_entropy + std::string(" [") + status + std::string("]"));
    glfwSetWindowTitle(m_window, statusString.c_str());
  }
}

void GlfwWrapper::toggleFullScreenMode(bool forceWindowMode)
{
  const bool isFullScreen = (nullptr != glfwGetWindowMonitor(m_window));

  if (forceWindowMode || isFullScreen)
  {
    // Restore windowed mode with backup of position and size:
    glfwSetWindowMonitor(
      m_window,
      nullptr,
      m_backupWindowPosX,
      m_backupWindowPosY,
      m_backupWindowWidth,
      m_backupWindowHeight,
      GLFW_DONT_CARE
    );
  }
  else if (!isFullScreen)
  {
    // Switch to full-screen mode after backing up position and size:
    glfwGetWindowPos(m_window, &m_backupWindowPosX, &m_backupWindowPosY);
    glfwGetWindowSize(m_window, &m_backupWindowWidth, &m_backupWindowHeight);

    GLFWmonitor* monitor = currentMonitor();
    if (!monitor)
    {
      spdlog::error("Null monitor upon setting full-screen mode.");
      return;
    }

    const GLFWvidmode* mode = glfwGetVideoMode(monitor);
    if (!mode)
    {
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

  int winPosX, winPosY, winWidth, winHeight;
  glfwGetWindowPos(m_window, &winPosX, &winPosY);
  glfwGetWindowSize(m_window, &winWidth, &winHeight);

  int numMonitors;
  GLFWmonitor** monitors = glfwGetMonitors(&numMonitors);

  for (int i = 0; i < numMonitors; ++i)
  {
    if (!monitors[i])
    {
      spdlog::debug("Monitor {} is null", i);
      continue;
    }

    const GLFWvidmode* mode = glfwGetVideoMode(monitors[i]);
    if (!mode)
    {
      spdlog::debug("Video mode for monitor {} is null", i);
      continue;
    }

    int monitorPosX, monitorPosY;
    glfwGetMonitorPos(monitors[i], &monitorPosX, &monitorPosY);

    const int monitorWidth = mode->width;
    const int monitorHeight = mode->height;

    const int overlapX = std::max(
      std::min(winPosX + winWidth, monitorPosX + monitorWidth) - std::max(winPosX, monitorPosX), 0
    );

    const int overlapY = std::max(
      std::min(winPosY + winHeight, monitorPosY + monitorHeight) - std::max(winPosY, monitorPosY), 0
    );

    const int overlap = overlapX * overlapY;

    if (largestOverlap < overlap)
    {
      largestOverlap = overlap;
      currentMonitor = monitors[i];
    }
  }

  return currentMonitor;
}
