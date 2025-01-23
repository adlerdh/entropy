#ifndef GL_SHADER_TYPE_H
#define GL_SHADER_TYPE_H

#include <glad/glad.h>

#include <cstdint>
#include <type_traits>

enum class ShaderType : uint32_t
{
  Vertex = GL_VERTEX_SHADER,
  Fragment = GL_FRAGMENT_SHADER,
  Geometry = GL_GEOMETRY_SHADER,
  TessControl = GL_TESS_CONTROL_SHADER,
  TessEvaluation = GL_TESS_EVALUATION_SHADER
  //    COMPUTE = GL_COMPUTE_SHADER
};

//using ShaderType_utype = std::underlying_type< ShaderType >::type;

#endif // GL_SHADER_TYPE_H
