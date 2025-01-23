#include "rendering/utility/gl/GLErrorChecker.h"
#include "common/Exception.hpp"

#include <spdlog/fmt/ostr.h>
#include <spdlog/spdlog.h>

#include <sstream>
#include <string>

void GLErrorChecker::operator()(const char* file, const char* function, int line)
{
  static const char* INVALID_ENUM_MSG = "Enumeration parameter not legal for function.";
  static const char* INVALID_VALUE_MSG = "Value parameter not legal for function.";
  static const char* INVALID_OPERATION_MSG
    = "Set of state not legal for parameters given to command.";
  static const char* STACK_OVERFLOW_MSG
    = "Stack pushing operation would overflow stack size limit.";
  static const char* STACK_UNDERFLOW_MSG
    = "Stack popping operation cannot be done; stack already at lowest point.";
  static const char* OUT_OF_MEMORY_MSG = "Memory cannot be allocated for operation.";
  static const char* INVALID_FRAMEBUFFER_OPERATION_MSG
    = "Attempt to read from or write/render to incomplete framebuffer.";
  //    static const char* CONTEXT_LOST_MSG = "OpenGL context has been lost due to a graphics card reset.";
  static const char* TABLE_TOO_LARGE_MSG
    = "Attempt to read from or write/render to a framebuffer that is not complete.";
  static const char* UNKNOWN_MSG = "Unknown error.";

  GLenum error;

  while (GL_NO_ERROR != (error = glGetError()))
  {
    const char* msg;

    switch (error)
    {
    case GL_INVALID_ENUM:
      msg = INVALID_ENUM_MSG;
      break;
    case GL_INVALID_VALUE:
      msg = INVALID_VALUE_MSG;
      break;
    case GL_INVALID_OPERATION:
      msg = INVALID_OPERATION_MSG;
      break;
    case GL_STACK_OVERFLOW:
      msg = STACK_OVERFLOW_MSG;
      break;
    case GL_STACK_UNDERFLOW:
      msg = STACK_UNDERFLOW_MSG;
      break;
    case GL_OUT_OF_MEMORY:
      msg = OUT_OF_MEMORY_MSG;
      break;
    case GL_INVALID_FRAMEBUFFER_OPERATION:
      msg = INVALID_FRAMEBUFFER_OPERATION_MSG;
      break;
      //      case GL_CONTEXT_LOST: msg = CONTEXT_LOST_MSG; break;
    case GL_TABLE_TOO_LARGE:
      msg = TABLE_TOO_LARGE_MSG;
      break;
    default:
      msg = UNKNOWN_MSG;
      break;
    }

    std::ostringstream ss;
    ss << "OpenGL error " << error << ": " << msg << std::ends;

    spdlog::error("{}", ss.str());

    throw Exception(ss.str(), file, function, line);
  }
}

void GLErrorChecker::operator()()
{
  // no-op
}
