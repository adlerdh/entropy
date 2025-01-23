#ifndef GLERRORCHECKER_H
#define GLERRORCHECKER_H

#include <glad/glad.h>

#ifdef NDEBUG
#define CHECK_GL_ERROR(checker) checker();
#else
#define CHECK_GL_ERROR(checker) checker((__FILE__), (__FUNCTION__), (__LINE__));
#endif

class GLErrorChecker final
{
public:
  GLErrorChecker() = default;
  ~GLErrorChecker() = default;

  void operator()(const char* file, const char* function, int line);
  void operator()();
};

#endif // GLERRORCHECKER_H
