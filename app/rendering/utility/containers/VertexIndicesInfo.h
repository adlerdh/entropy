#pragma once

#include "rendering/utility/gl/GLDrawTypes.h"

#include <cstdint>

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
  VertexIndicesInfo(
    IndexType indexType,
    PrimitiveMode primitiveMode,
    std::uint64_t indexCount,
    std::uint64_t indexOffset);

  IndexType indexType() const;
  PrimitiveMode primitiveMode() const;
  std::uint64_t indexCount() const;
  std::uint64_t indexOffset() const;

  void setIndexCount(std::uint64_t indexCount);

private:
  IndexType m_indexType;
  PrimitiveMode m_primitiveMode;
  std::uint64_t m_indexCount;
  std::uint64_t m_indexOffset;
};
