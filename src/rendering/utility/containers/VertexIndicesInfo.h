#ifndef VERTEX_INDICES_INFO_H
#define VERTEX_INDICES_INFO_H

#include "rendering/utility/gl/GLDrawTypes.h"

class VertexIndicesInfo
{
public:
  VertexIndicesInfo(
    IndexType indexType, PrimitiveMode primitiveMode, uint64_t indexCount, uint64_t indexOffset
  );

  IndexType indexType() const;
  PrimitiveMode primitiveMode() const;
  uint64_t indexCount() const;
  uint64_t indexOffset() const;

  void setIndexCount(uint64_t n);

private:
  IndexType m_indexType;
  PrimitiveMode m_primitiveMode;
  uint64_t m_indexCount;
  uint64_t m_indexOffset;
};

#endif // VERTEX_INDICES_INFO_H
