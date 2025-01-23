#ifndef VERTEX_ATTRIBUTE_INFO_H
#define VERTEX_ATTRIBUTE_INFO_H

#include "rendering/utility/gl/GLBufferTypes.h"

class VertexAttributeInfo
{
public:
  VertexAttributeInfo(
    BufferComponentType componentType,
    BufferNormalizeValues normalizeValues,
    int numComponents,
    int strideInBytes,
    int offsetInBytes,
    uint64_t vertexCount
  );

  BufferComponentType componentType() const;
  BufferNormalizeValues normalizeValues() const;
  int numComponents() const;
  int strideInBytes() const;
  int offsetInBytes() const;
  uint64_t vertexCount() const;

  void setVertexCount(uint64_t n);

private:
  BufferComponentType m_componentType;
  BufferNormalizeValues m_normalizeValues;
  int m_numComponents;
  int m_strideInBytes;
  int m_offsetInBytes;
  uint64_t m_vertexCount;
};

#endif // VERTEX_ATTRIBUTE_INFO_H
