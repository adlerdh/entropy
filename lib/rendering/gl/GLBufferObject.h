#pragma once

#include "rendering/gl/GLBufferTypes.h"
#include "rendering/gl/GLErrorChecker.h"

#include <glad/glad.h>

#include <cstddef>
#include <set>

/**
 * @brief RAII wrapper around an OpenGL buffer object.
 *
 * The wrapper owns one GL buffer name and remembers its target, usage hint, and current allocated byte size. Methods
 * bind the buffer before mutating or reading it, so callers do not need to bind it separately.
 */
class GLBufferObject final
{
public:
  /**
   * @param type OpenGL buffer target this object is bound to.
   * @param usagePattern Expected buffer data usage hint.
   */
  GLBufferObject(const BufferType& type, const BufferUsagePattern& usage);

  GLBufferObject(const GLBufferObject&) = delete;
  GLBufferObject& operator=(const GLBufferObject&) = delete;

  GLBufferObject(GLBufferObject&& other) noexcept;
  GLBufferObject& operator=(GLBufferObject&& other) noexcept;

  /// Delete the GL buffer name and GPU storage if still owned.
  ~GLBufferObject();

  /// Generate the GL buffer name if needed.
  void generate();

  /// Delete the GL buffer name and reset local state.
  void destroy();

  /// Bind this buffer to its configured target.
  void bind() const;

  void unbind() const;

  /**
   * @brief Allocate mutable storage and optionally initialize it with CPU data.
   *
   * @param sizeInBytes New buffer storage size.
   * @param data Optional CPU data copied into the new storage. If null, storage contents are undefined.
   */
  void allocate(std::size_t sizeInBytes, const GLvoid* data);

  /**
   * @brief Replace a byte range in the existing buffer storage.
   *
   * @param offset Byte offset where replacement begins.
   * @param sizeInBytes Number of bytes to replace.
   * @param data CPU data copied into the selected byte range.
   */
  void write(std::size_t offset, std::size_t sizeInBytes, const GLvoid* data);

  /**
   * @brief Copy a byte range from GPU buffer storage into CPU memory.
   *
   * @param offset Byte offset where the read begins.
   * @param sizeInBytes Number of bytes to read.
   * @param data Destination CPU buffer.
   */
  void read(std::size_t offset, std::size_t sizeInBytes, GLvoid* data) const;

  /**
   * @brief Map the full buffer into client address space.
   *
   * @param access Access policy requested for the mapped storage.
   * @return Mapped pointer, or null if OpenGL cannot map the buffer.
   */
  void* map(const BufferMapAccessPolicy& access);

  /**
   * @brief Map part of the buffer into client address space.
   *
   * @param offset Byte offset where the mapped range begins.
   * @param length Number of bytes in the mapped range.
   * @param accessFlags Access flags requested for the mapped range.
   * @return Mapped pointer, or null if OpenGL cannot map the buffer.
   */
  void* mapRange(std::size_t offset, std::size_t length, const std::set<BufferMapRangeAccessFlag>& accessFlags);

  /// Flush writes to a subrange of a mapping created with `FlushExplicitBit`.
  /// The offset is relative to the start of the mapped range, matching `glFlushMappedBufferRange()`.
  void flushMappedRange(std::size_t offset, std::size_t length);

  /// Unmap a previously mapped buffer range.
  bool unmap();

  /// Copy bytes between two buffer objects using OpenGL's copy buffer targets.
  static void copyData(
    GLBufferObject& readBuffer,
    const GLBufferObject& writeBuffer,
    std::size_t readOffset,
    std::size_t writeOffset,
    std::size_t sizeInBytes);

  GLuint id() const;

  BufferType type() const;

  BufferUsagePattern usagePattern() const;

  /// Return the currently allocated buffer size in bytes.
  std::size_t size() const;

private:
  GLErrorChecker m_errorChecker;

  GLuint m_id;

  BufferType m_type;
  GLenum m_typeEnum;
  BufferUsagePattern m_usagePattern;

  std::size_t m_bufferSizeInBytes;
  bool m_isMapped = false;
  std::size_t m_mappedLength = 0;
  bool m_requiresExplicitFlush = false;
};
