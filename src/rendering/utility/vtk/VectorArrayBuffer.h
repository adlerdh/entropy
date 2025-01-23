#ifndef VECTOR_ARRAY_BUFFER_H
#define VECTOR_ARRAY_BUFFER_H

#include <vtkType.h>

#include <memory>

namespace vtkconvert
{

template<typename T>
class VectorArrayBuffer
{
public:
  VectorArrayBuffer()
    : m_vectorCount(0)
    , m_bufferLength(0)
    , m_bufferByteCount(0)
    , m_buffer(nullptr)
  {
  }

  VectorArrayBuffer(VectorArrayBuffer& other) = delete;
  VectorArrayBuffer& operator=(const VectorArrayBuffer& other) = delete;

  VectorArrayBuffer(VectorArrayBuffer&& other)
    : m_vectorCount(other.m_vectorCount)
    , m_bufferLength(other.m_bufferLength)
    , m_bufferByteCount(other.m_bufferByteCount)
    , m_buffer(std::move(other.m_buffer))
  {
  }

  VectorArrayBuffer& operator=(VectorArrayBuffer&& other)
  {
    if (this != &other)
    {
      m_vectorCount = other.m_vectorCount;
      m_bufferLength = other.m_bufferLength;
      m_bufferByteCount = other.m_bufferByteCount;
      m_buffer.reset(std::move(other.m_buffer));
    }
    return *this;
  }

  ~VectorArrayBuffer() = default;

  std::size_t vectorCount() const { return m_vectorCount; }
  std::size_t length() const { return m_bufferLength; }
  std::size_t byteCount() const { return m_bufferByteCount; }

  const void* buffer() const { return m_buffer.get(); }

protected:
  std::size_t m_vectorCount;
  std::size_t m_bufferLength;
  std::size_t m_bufferByteCount;
  std::unique_ptr<T[]> m_buffer;
};

} // namespace vtkconvert

#endif // VECTOR_ARRAY_BUFFER_H
