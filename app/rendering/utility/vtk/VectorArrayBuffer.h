#pragma once

#include <vtkType.h>

#include <memory>

namespace vtkconvert
{

/**
 * @brief Owning contiguous buffer for fixed-width vector arrays extracted from VTK data.
 *
 * The buffer stores vectors flattened as scalar values so they can be uploaded directly to OpenGL buffers. `length()`
 * returns the scalar count, while `vectorCount()` returns the number of logical vectors.
 */
template<typename T>
class VectorArrayBuffer
{
public:
  VectorArrayBuffer() : m_vectorCount(0), m_bufferLength(0), m_bufferByteCount(0), m_buffer(nullptr) {}

  VectorArrayBuffer(const VectorArrayBuffer& other) = delete;
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
    if (this != &other) {
      m_vectorCount = other.m_vectorCount;
      m_bufferLength = other.m_bufferLength;
      m_bufferByteCount = other.m_bufferByteCount;
      m_buffer = std::move(other.m_buffer);
    }
    return *this;
  }

  ~VectorArrayBuffer() = default;

  std::size_t vectorCount() const
  {
    return m_vectorCount;
  }
  std::size_t length() const
  {
    return m_bufferLength;
  }
  std::size_t byteCount() const
  {
    return m_bufferByteCount;
  }

  const void* buffer() const
  {
    return m_buffer.get();
  }

protected:
  std::size_t m_vectorCount; //!< Number of logical vectors in the buffer

  std::size_t m_bufferLength; //!< Number of scalar values in the flattened buffer

  std::size_t m_bufferByteCount; //!< Number of bytes in the flattened buffer

  std::unique_ptr<T[]> m_buffer; //!< Flattened scalar storage
};

} // namespace vtkconvert
