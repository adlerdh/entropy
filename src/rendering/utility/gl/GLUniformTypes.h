#ifndef GL_UNIFORM_TYPES_H
#define GL_UNIFORM_TYPES_H

#include <glad/glad.h>

#include <cstdint>

enum class UniformType : uint32_t
{
  Bool = GL_BOOL,
  Int = GL_INT,
  UInt = GL_UNSIGNED_INT,
  Float = GL_FLOAT,
  Double = GL_DOUBLE,
  IVec2 = GL_INT_VEC2,
  Vec2 = GL_FLOAT_VEC2,
  Vec3 = GL_FLOAT_VEC3,
  Vec4 = GL_FLOAT_VEC4,
  Mat2 = GL_FLOAT_MAT2,
  Mat3 = GL_FLOAT_MAT3,
  Mat4 = GL_FLOAT_MAT4,

  // Special types defined by Entropy:
  Sampler = 1,
  SamplerVector = 2,
  FloatVector = 3,
  Vec2Vector = 4,
  Mat4Vector = 5,
  Vec3Vector = 6,
  FloatArray2 = 7,
  FloatArray3 = 8,
  FloatArray4 = 9,
  FloatArray5 = 10,
  UIntArray5 = 11,
  Vec3Array8 = 12,

  Undefined = 0
};

#endif // GL_UNIFORM_TYPES_H
