#include "rendering/utility/gl/GLErrorChecker.h"
#include "common/Exception.hpp"

#include <glad/glad.h>

#include <spdlog/fmt/ostr.h>
#include <spdlog/spdlog.h>

#include <sstream>
#include <string>

namespace
{
constexpr const char* k_invalidEnumMessage = "Enumeration parameter not legal for function.";
constexpr const char* k_invalidValueMessage = "Value parameter not legal for function.";
constexpr const char* k_invalidOperationMessage = "Set of state not legal for parameters given to command.";
constexpr const char* k_stackOverflowMessage = "Stack pushing operation would overflow stack size limit.";
constexpr const char* k_stackUnderflowMessage =
  "Stack popping operation cannot be done; stack already at lowest point.";
constexpr const char* k_outOfMemoryMessage = "Memory cannot be allocated for operation.";
constexpr const char* k_invalidFramebufferOperationMessage =
  "Attempt to read from or write/render to incomplete framebuffer.";
constexpr const char* k_tableTooLargeMessage =
  "Attempt to read from or write/render to a framebuffer that is not complete.";
constexpr const char* k_unknownMessage = "Unknown error.";
} // namespace

void GLErrorChecker::operator()(const char* file, const char* function, int line) const
{
  GLenum error = 0;

  while (GL_NO_ERROR != (error = glGetError())) {
    const char* msg = nullptr;

    switch (error) {
      case GL_INVALID_ENUM:
        msg = k_invalidEnumMessage;
        break;
      case GL_INVALID_VALUE:
        msg = k_invalidValueMessage;
        break;
      case GL_INVALID_OPERATION:
        msg = k_invalidOperationMessage;
        break;
      case GL_STACK_OVERFLOW:
        msg = k_stackOverflowMessage;
        break;
      case GL_STACK_UNDERFLOW:
        msg = k_stackUnderflowMessage;
        break;
      case GL_OUT_OF_MEMORY:
        msg = k_outOfMemoryMessage;
        break;
      case GL_INVALID_FRAMEBUFFER_OPERATION:
        msg = k_invalidFramebufferOperationMessage;
        break;
        //      case GL_CONTEXT_LOST: msg = CONTEXT_LOST_MSG; break;
      case GL_TABLE_TOO_LARGE:
        msg = k_tableTooLargeMessage;
        break;
      default:
        msg = k_unknownMessage;
        break;
    }

    std::ostringstream ss;
    ss << "OpenGL error " << error << ": " << msg << std::ends;

    spdlog::error("{}", ss.str());

    throwDebug(ss.str(), file, function, line);
  }
}

void GLErrorChecker::operator()() const
{
  // no-op
}
