#pragma once

#include "rendering/utility/gl/GLBufferTypes.h"

/**
 * @brief Describes one vertex attribute stream inside a bound vertex buffer.
 *
 * The object stores the metadata needed by `GLVertexArrayObject` to call `glVertexAttribPointer()` or
 * `glVertexAttribIPointer()`: component type, component count, byte stride, byte offset, normalization flag, and the
 * number of vertices represented by the buffer.
 */
class VertexAttributeInfo
{
public:
  /**
   * @param componentType OpenGL scalar type stored for each attribute component.
   * @param normalizeValues Whether fixed-point components should be normalized when fetched by the shader.
   * @param numComponents Number of scalar components per vertex attribute.
   * @param strideInBytes Byte stride between consecutive vertex attributes.
   * @param offsetInBytes Byte offset of the first vertex attribute inside the bound buffer.
   * @param vertexCount Number of vertices described by this attribute stream.
   */
  VertexAttributeInfo(
    BufferComponentType componentType,
    BufferNormalizeValues normalizeValues,
    int numComponents,
    int strideInBytes,
    int offsetInBytes,
    uint64_t vertexCount);

  BufferComponentType componentType() const;
  BufferNormalizeValues normalizeValues() const;
  int numComponents() const;
  int strideInBytes() const;
  int offsetInBytes() const;
  uint64_t vertexCount() const;

  void setVertexCount(uint64_t vertexCount);

private:
  BufferComponentType m_componentType;
  BufferNormalizeValues m_normalizeValues;
  int m_numComponents;
  int m_strideInBytes;
  int m_offsetInBytes;
  uint64_t m_vertexCount;
};
