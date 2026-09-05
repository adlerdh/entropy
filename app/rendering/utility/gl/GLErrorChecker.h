#pragma once

#include <glad/glad.h>

#define CHECK_GL_ERROR(checker) (checker)(__FILE__, __func__, __LINE__)

/**
 * @brief Throws when OpenGL reports an error on the current context.
 *
 * Checks remain enabled in release builds because a queued OpenGL error can otherwise be reported much later at an
 * unrelated call site, making driver failures difficult to diagnose.
 */
class GLErrorChecker final
{
public:
  GLErrorChecker() = default;
  ~GLErrorChecker() = default;

  void operator()(const char* file, const char* function, int line) const;
};
