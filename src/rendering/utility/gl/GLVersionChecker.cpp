#include "rendering/utility/gl/GLVersionChecker.h"

#include "common/Exception.hpp"

#include <glad/glad.h>

#include <spdlog/fmt/ostr.h>
#include <spdlog/spdlog.h>

#include <sstream>

GLVersionChecker::GLVersionChecker()
{
  // Major version number of the OpenGL API supported by the current context:
  GLint majorVersion;
  glGetIntegerv(GL_MAJOR_VERSION, &majorVersion);

  // Minor version number of the OpenGL API supported by the current context:
  GLint minorVersion;
  glGetIntegerv(GL_MINOR_VERSION, &minorVersion);

  if (majorVersion < 3 || (majorVersion == 3 && minorVersion < 3))
  {
    spdlog::critical("OpenGL version {}.{} is too low and not supported.", majorVersion, minorVersion);
    spdlog::critical("The minimum required OpenGL version is 3.3");
    throw_debug("Invalid OpenGL version found")
  }

  // Profile mask used to create the context:
  GLint contextProfileMask;
  glGetIntegerv(GL_CONTEXT_PROFILE_MASK, &contextProfileMask);

  std::ostringstream ss;

  ss << "OpenGL context information:" << std::endl;
  ss << "\tVersion: " << glGetString(GL_VERSION);

  if (contextProfileMask & GL_CONTEXT_CORE_PROFILE_BIT)
  {
    ss << " (core profile)";
  }
  else if (contextProfileMask & GL_CONTEXT_COMPATIBILITY_PROFILE_BIT)
  {
    ss << " (compatibility profile)";
  }

  ss << std::endl;
  ss << "\tVendor: " << glGetString(GL_VENDOR) << std::endl;
  ss << "\tRenderer: " << glGetString(GL_RENDERER) << std::endl;

  spdlog::info("{}", ss.str());

  CHECK_GL_ERROR(m_errorChecker)
}
