#pragma once

#include <glad/glad.h>

#ifdef NDEBUG
#define CHECK_GL_ERROR(checker) checker();
#else
#define CHECK_GL_ERROR(checker) checker((__FILE__), (__FUNCTION__), (__LINE__));
#endif

/**
 * @brief Throws when OpenGL reports an error on the current context.
 *
 * The debug overload includes source location in the diagnostic. The release overload omits it to avoid carrying file
 * and function strings through release builds.
 */
class GLErrorChecker final
{
public:
  GLErrorChecker() = default;
  ~GLErrorChecker() = default;

  void operator()(const char* file, const char* function, int line) const;
  void operator()() const;
};
