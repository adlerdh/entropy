#pragma once

#include "rendering/utility/gl/GLErrorChecker.h"

/**
 * @brief Validates that the active OpenGL context satisfies Entropy's rendering requirements.
 *
 * Construction performs the check and throws/logs through the standard GL error path if the context is unsuitable.
 */
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
