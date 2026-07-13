#pragma once

#include <glad/glad.h>

#include <cstdint>

/**
 * @brief OpenGL shader object types supported by the wrapper layer.
 */
enum class ShaderType : uint32_t
{
  Vertex = GL_VERTEX_SHADER,
  Fragment = GL_FRAGMENT_SHADER,
  Geometry = GL_GEOMETRY_SHADER,
  TessControl = GL_TESS_CONTROL_SHADER,
  TessEvaluation = GL_TESS_EVALUATION_SHADER
  //    COMPUTE = GL_COMPUTE_SHADER
};
