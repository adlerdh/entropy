#pragma once

#include <glad/glad.h>

#include <cstdint>

/**
 * @brief Primitive topology values accepted by draw calls.
 */
enum class PrimitiveMode : uint32_t
{
  Points = GL_POINTS,                                   //!< Independent points
  Lines = GL_LINES,                                     //!< Independent line segments
  LineLoop = GL_LINE_LOOP,                              //!< Connected loop of line segments
  LineStrip = GL_LINE_STRIP,                            //!< Connected strip of line segments
  Triangles = GL_TRIANGLES,                             //!< Independent triangles
  TriangleStrip = GL_TRIANGLE_STRIP,                    //!< Connected triangle strip
  TriangleFan = GL_TRIANGLE_FAN,                        //!< Connected triangle fan
  LinesAdjacency = GL_LINES_ADJACENCY,                  //!< Line segments with adjacency vertices
  LineStripAdjacency = GL_LINE_STRIP_ADJACENCY,         //!< Line strip with adjacency vertices
  TrianglesAdjacency = GL_TRIANGLES_ADJACENCY,          //!< Triangles with adjacency vertices
  TriangleStripAdjacency = GL_TRIANGLE_STRIP_ADJACENCY, //!< Triangle strip with adjacency vertices
  Patches = GL_PATCHES                                  //!< Tessellation patch primitives
};

/**
 * @brief Scalar index types accepted by indexed draw calls.
 */
enum class IndexType : uint32_t
{
  UInt8 = GL_UNSIGNED_BYTE,   //!< 8-bit unsigned indices
  UInt16 = GL_UNSIGNED_SHORT, //!< 16-bit unsigned indices
  UInt32 = GL_UNSIGNED_INT    //!< 32-bit unsigned indices
};
