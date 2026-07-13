#pragma once

#include <glad/glad.h>

/**
 * @brief Small diagnostics helper for OpenGL context and shader debugging information.
 */
class ShaderInfo
{
public:
  ShaderInfo();
  ~ShaderInfo() = default;

  /// Check and report OpenGL errors for a source file and line.
  bool checkForOpenGLError(const char* file, int line);

  /// Log basic OpenGL renderer, version, and optionally extension information.
  void dumpGLInfo(bool dumpExtensions = false);

#ifdef USING_GL_VERSION_4_3
  /// OpenGL debug message callback used when GL 4.3 debug output is available.
  void debugCallback(
    GLenum source,
    GLenum type,
    GLuint id,
    GLenum severity,
    GLsizei length,
    const GLchar* msg,
    const void* param);
#endif
};
