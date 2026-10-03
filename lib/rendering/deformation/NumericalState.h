#pragma once

#include "rendering/gl/OpenGLStateGuard.h"

#include <glad/glad.h>

#include <array>
#include <optional>
#include <vector>

namespace rendering::deformation::detail
{
/** @brief Internal guard for the extra pixel-transfer and raster state used by numerical passes. */
class NumericalState final
{
public:
  NumericalState();
  ~NumericalState();
  NumericalState(const NumericalState&) = delete;
  NumericalState& operator=(const NumericalState&) = delete;

private:
  std::optional<OpenGLStateGuard> m_common;
  struct DrawBufferState
  {
    GLboolean blend = GL_FALSE;
    std::array<GLboolean, 4> colorMask{};
  };
  std::vector<DrawBufferState> m_drawBuffers;
  std::array<GLint, 2> m_samplers{};
  std::array<GLint, 16> m_pixelStore{};
  std::array<GLboolean, 6> m_enabled{};
  std::vector<GLboolean> m_clipDistances;
  GLint m_packBuffer = 0;
  GLint m_unpackBuffer = 0;
};
} // namespace rendering::deformation::detail
