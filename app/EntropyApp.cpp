#include "EntropyApp.h"
#include "BuildStamp.h"

#include <spdlog/spdlog.h>

EntropyApp::EntropyApp()
  : m_imageLoadCancelled(false)
  , m_imagesReady(false)
  , m_imageLoadFailed(false)
  , m_glfw(this, GL_VERSION_MAJOR, GL_VERSION_MINOR)
  , m_rendering(m_data)
  , m_callbackHandler(m_data, m_glfw, m_rendering)
  , m_itkSnapSync(m_data)
  , m_entropyInstanceSync(m_data)
  , m_imgui(m_glfw.window(), m_data, m_callbackHandler)
{
  spdlog::debug("Begin constructing application");
  setCallbacks();
  spdlog::debug("Done constructing application");
}

EntropyApp::~EntropyApp()
{
  if (m_futureLoadProject.valid()) {
    m_futureLoadProject.wait();
  }
  if (m_futureDiscoverDicom.valid()) {
    m_futureDiscoverDicom.wait();
  }

  Rendering::prepareForShutdown();
}

CallbackHandler& EntropyApp::callbackHandler()
{
  return m_callbackHandler;
}

const AppData& EntropyApp::appData() const
{
  return m_data;
}

AppData& EntropyApp::appData()
{
  return m_data;
}

const GlfwWrapper& EntropyApp::glfw() const
{
  return m_glfw;
}

GlfwWrapper& EntropyApp::glfw()
{
  return m_glfw;
}

const ImGuiWrapper& EntropyApp::imgui() const
{
  return m_imgui;
}

ImGuiWrapper& EntropyApp::imgui()
{
  return m_imgui;
}

const WindowData& EntropyApp::windowData() const
{
  return m_data.windowData();
}

WindowData& EntropyApp::windowData()
{
  return m_data.windowData();
}
