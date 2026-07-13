#pragma once

#include "rendering/utility/gl/GLDrawTypes.h"

/**
 * @brief Describes indexed draw parameters stored in an element/index buffer.
 */
class VertexIndicesInfo
{
public:
  /**
   * @param indexType OpenGL scalar type used for each index.
   * @param primitiveMode Primitive topology used when drawing the indices.
   * @param indexCount Number of indices to draw.
   * @param indexOffset Byte offset of the first index inside the bound index buffer.
   */
  VertexIndicesInfo(IndexType indexType, PrimitiveMode primitiveMode, uint64_t indexCount, uint64_t indexOffset);

  IndexType indexType() const;
  PrimitiveMode primitiveMode() const;
  uint64_t indexCount() const;
  uint64_t indexOffset() const;

  void setIndexCount(uint64_t indexCount);

private:
  IndexType m_indexType;
  PrimitiveMode m_primitiveMode;
  uint64_t m_indexCount;
  uint64_t m_indexOffset;
};
