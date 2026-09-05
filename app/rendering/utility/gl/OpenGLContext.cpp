#include "rendering/utility/gl/OpenGLContext.h"

#include "rendering/utility/gl/GLErrorChecker.h"

#include "common/Exception.hpp"

#include <glad/glad.h>

#include <spdlog/spdlog.h>

#include <sstream>
#include <string_view>

namespace
{

void GLAPIENTRY logOpenGLDebugMessage(
  const GLenum source,
  const GLenum type,
  const GLuint id,
  const GLenum severity,
  const GLsizei length,
  const GLchar* message,
  const void* /*userData*/)
{
  if (severity == GL_DEBUG_SEVERITY_NOTIFICATION || message == nullptr) {
    return;
  }

  const std::string_view text{
    message,
    length > 0 ? static_cast<std::size_t>(length) : std::char_traits<char>::length(message)};
  if (severity == GL_DEBUG_SEVERITY_HIGH || type == GL_DEBUG_TYPE_ERROR) {
    spdlog::error("OpenGL debug message {} (source {}, type {}): {}", id, source, type, text);
  }
  else if (severity == GL_DEBUG_SEVERITY_MEDIUM) {
    spdlog::warn("OpenGL debug message {} (source {}, type {}): {}", id, source, type, text);
  }
  else {
    spdlog::debug("OpenGL debug message {} (source {}, type {}): {}", id, source, type, text);
  }
}

void enableOpenGLDebugLogging()
{
  if (!GLAD_GL_KHR_debug || glDebugMessageCallback == nullptr) {
    spdlog::debug("OpenGL KHR_debug diagnostics are unavailable");
    return;
  }

  glEnable(GL_DEBUG_OUTPUT);
  glDebugMessageCallback(logOpenGLDebugMessage, nullptr);
  glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DEBUG_SEVERITY_NOTIFICATION, 0, nullptr, GL_FALSE);
  CHECK_GL_ERROR(GLErrorChecker{});
  spdlog::debug("Enabled OpenGL KHR_debug diagnostics");
}

} // namespace

void validateOpenGLContext()
{
  GLint majorVersion = 0;
  GLint minorVersion = 0;
  GLint contextProfileMask = 0;
  glGetIntegerv(GL_MAJOR_VERSION, &majorVersion);
  glGetIntegerv(GL_MINOR_VERSION, &minorVersion);
  glGetIntegerv(GL_CONTEXT_PROFILE_MASK, &contextProfileMask);
  CHECK_GL_ERROR(GLErrorChecker{});

  if (majorVersion < 3 || (majorVersion == 3 && minorVersion < 3)) {
    spdlog::critical(
      "OpenGL version {}.{} is unsupported. Entropy requires OpenGL 3.3 or newer",
      majorVersion,
      minorVersion);
    throwDebug("The current OpenGL context does not meet Entropy's minimum requirements");
  }
  if ((contextProfileMask & GL_CONTEXT_CORE_PROFILE_BIT) == 0) {
    spdlog::critical("Entropy requires an OpenGL core-profile context");
    throwDebug("The current OpenGL context is not a core-profile context");
  }

  const auto glString = [](const GLenum name) {
    const GLubyte* value = glGetString(name);
    return value ? reinterpret_cast<const char*>(value) : "unavailable";
  };

  std::ostringstream message;
  message << "OpenGL context information:\n"
          << "\tVersion: " << glString(GL_VERSION) << " (core profile)\n"
          << "\tVendor: " << glString(GL_VENDOR) << '\n'
          << "\tRenderer: " << glString(GL_RENDERER) << '\n'
          << "\tGLSL: " << glString(GL_SHADING_LANGUAGE_VERSION);
  spdlog::info("{}", message.str());
  enableOpenGLDebugLogging();
}
