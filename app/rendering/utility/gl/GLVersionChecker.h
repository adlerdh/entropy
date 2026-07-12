#pragma once

#include "rendering/utility/gl/GLErrorChecker.h"

class GLVersionChecker final
{
public:
  GLVersionChecker();

  GLVersionChecker(const GLVersionChecker&) = delete;
  GLVersionChecker& operator=(const GLVersionChecker&) = delete;

  GLVersionChecker(GLVersionChecker&&) = default;
  GLVersionChecker& operator=(GLVersionChecker&&) = default;

  ~GLVersionChecker() = default;

private:
  GLErrorChecker m_errorChecker;
};
