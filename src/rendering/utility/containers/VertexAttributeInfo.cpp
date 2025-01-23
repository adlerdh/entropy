#include "rendering/utility/containers/VertexAttributeInfo.h"

#include <utility>

VertexAttributeInfo::VertexAttributeInfo(
  BufferComponentType componentType,
  BufferNormalizeValues normalizeValues,
  int numComponents,
  int strideInBytes,
  int offsetInBytes,
  uint64_t vertexCount
)
  : m_componentType(std::move(componentType))
  , m_normalizeValues(std::move(normalizeValues))
  , m_numComponents(numComponents)
  , m_strideInBytes(strideInBytes)
  , m_offsetInBytes(offsetInBytes)
  , m_vertexCount(vertexCount)
{
}

BufferComponentType VertexAttributeInfo::componentType() const
{
  return m_componentType;
}

BufferNormalizeValues VertexAttributeInfo::normalizeValues() const
{
  return m_normalizeValues;
}

int VertexAttributeInfo::numComponents() const
{
  return m_numComponents;
}

int VertexAttributeInfo::strideInBytes() const
{
  return m_strideInBytes;
}

int VertexAttributeInfo::offsetInBytes() const
{
  return m_offsetInBytes;
}

uint64_t VertexAttributeInfo::vertexCount() const
{
  return m_vertexCount;
}

void VertexAttributeInfo::setVertexCount(uint64_t n)
{
  m_vertexCount = n;
}
