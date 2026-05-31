#ifndef GLDRAWTYPES_H
#define GLDRAWTYPES_H

#include <glad/glad.h>

#include <cstdint>

enum class PrimitiveMode : uint32_t
{
  Points = GL_POINTS,
  Lines = GL_LINES,
  LineLoop = GL_LINE_LOOP,
  LineStrip = GL_LINE_STRIP,
  Triangles = GL_TRIANGLES,
  TriangleStrip = GL_TRIANGLE_STRIP,
  TriangleFan = GL_TRIANGLE_FAN,
  LinesAdjacency = GL_LINES_ADJACENCY,
  LineStripAdjacency = GL_LINE_STRIP_ADJACENCY,
  TrianglesAdjacency = GL_TRIANGLES_ADJACENCY,
  TriangleStripAdjacency = GL_TRIANGLE_STRIP_ADJACENCY,
  Patches = GL_PATCHES
};

enum class IndexType : uint32_t
{
  UInt8 = GL_UNSIGNED_BYTE,
  UInt16 = GL_UNSIGNED_SHORT,
  UInt32 = GL_UNSIGNED_INT
};

#endif // GLDRAWTYPES_H
