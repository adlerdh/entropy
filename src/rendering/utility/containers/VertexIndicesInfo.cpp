#include "rendering/utility/containers/VertexIndicesInfo.h"

#include <utility>

VertexIndicesInfo::VertexIndicesInfo(
  IndexType indexType, PrimitiveMode primitiveMode, uint64_t indexCount, uint64_t indexOffset
)
  : m_indexType(std::move(indexType))
  , m_primitiveMode(std::move(primitiveMode))
  , m_indexCount(indexCount)
  , m_indexOffset(indexOffset)
{
}

IndexType VertexIndicesInfo::indexType() const
{
  return m_indexType;
}

PrimitiveMode VertexIndicesInfo::primitiveMode() const
{
  return m_primitiveMode;
}

uint64_t VertexIndicesInfo::indexCount() const
{
  return m_indexCount;
}

uint64_t VertexIndicesInfo::indexOffset() const
{
  return m_indexOffset;
}

void VertexIndicesInfo::setIndexCount(uint64_t n)
{
  m_indexCount = n;
}
