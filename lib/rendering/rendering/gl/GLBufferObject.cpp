#include "rendering/gl/GLBufferObject.h"
#include "rendering/helpers/UnderlyingEnumType.h"

#include "common/Exception.hpp"

#include <glad/glad.h>

#include <limits>
#include <numeric>
#include <string>
#include <utility>

namespace
{

GLenum bufferBindingQuery(const GLenum target)
{
  switch (target) {
    case GL_ARRAY_BUFFER:
      return GL_ARRAY_BUFFER_BINDING;
    case GL_COPY_READ_BUFFER:
      // Core OpenGL uses the target token itself as the generic binding query for copy buffers.
      return GL_COPY_READ_BUFFER;
    case GL_COPY_WRITE_BUFFER:
      return GL_COPY_WRITE_BUFFER;
    case GL_DRAW_INDIRECT_BUFFER:
      return GL_DRAW_INDIRECT_BUFFER_BINDING;
    case GL_ELEMENT_ARRAY_BUFFER:
      return GL_ELEMENT_ARRAY_BUFFER_BINDING;
    case GL_PIXEL_PACK_BUFFER:
      return GL_PIXEL_PACK_BUFFER_BINDING;
    case GL_PIXEL_UNPACK_BUFFER:
      return GL_PIXEL_UNPACK_BUFFER_BINDING;
    case GL_TEXTURE_BUFFER:
      // GL_TEXTURE_BINDING_BUFFER queries the texture object, not the buffer bound to GL_TEXTURE_BUFFER.
      return GL_TEXTURE_BUFFER;
    case GL_TRANSFORM_FEEDBACK_BUFFER:
      return GL_TRANSFORM_FEEDBACK_BUFFER_BINDING;
    case GL_UNIFORM_BUFFER:
      return GL_UNIFORM_BUFFER_BINDING;
    default:
      throwDebug("Unsupported OpenGL buffer target");
  }
}

class ScopedBufferBinding
{
public:
  ScopedBufferBinding(const GLenum target, const GLuint buffer) : m_target(target)
  {
    glGetIntegerv(bufferBindingQuery(target), &m_previousBuffer);
    glBindBuffer(target, buffer);
  }

  ScopedBufferBinding(const ScopedBufferBinding&) = delete;
  ScopedBufferBinding& operator=(const ScopedBufferBinding&) = delete;

  ~ScopedBufferBinding()
  {
    glBindBuffer(m_target, static_cast<GLuint>(m_previousBuffer));
  }

private:
  GLenum m_target;
  GLint m_previousBuffer = 0;
};

GLsizeiptr checkedBufferSize(const std::size_t sizeInBytes)
{
  if (sizeInBytes > static_cast<std::size_t>(std::numeric_limits<GLsizeiptr>::max())) {
    throwDebug("OpenGL buffer size exceeds GLsizeiptr range");
  }
  return static_cast<GLsizeiptr>(sizeInBytes);
}

GLintptr checkedBufferOffset(const std::size_t offset)
{
  if (offset > static_cast<std::size_t>(std::numeric_limits<GLintptr>::max())) {
    throwDebug("OpenGL buffer offset exceeds GLintptr range");
  }
  return static_cast<GLintptr>(offset);
}

void validateBufferRange(
  const GLuint id,
  const std::size_t allocatedSize,
  const std::size_t offset,
  const std::size_t sizeInBytes,
  const GLvoid* data,
  const bool requireData,
  const char* operation)
{
  if (id == 0u) {
    throwDebug(std::string("Cannot ") + operation + " an OpenGL buffer before generating it");
  }
  if (offset > allocatedSize || sizeInBytes > allocatedSize - offset) {
    throwDebug(std::string("OpenGL buffer ") + operation + " range exceeds allocated storage");
  }
  if (requireData && sizeInBytes > 0u && data == nullptr) {
    throwDebug(std::string("OpenGL buffer ") + operation + " data pointer cannot be null");
  }
}

} // namespace

GLBufferObject::GLBufferObject(const BufferType& type, const BufferUsagePattern& usage)
  : m_id(0), m_type(type), m_typeEnum(underlyingType(type)), m_usagePattern(usage), m_bufferSizeInBytes(0)
{
}

GLBufferObject::~GLBufferObject()
{
  destroy();
}

GLBufferObject::GLBufferObject(GLBufferObject&& other) noexcept
  : m_id(other.m_id)
  , m_type(other.m_type)
  , m_typeEnum(underlyingType(other.m_type))
  , m_usagePattern(other.m_usagePattern)
  , m_bufferSizeInBytes(other.m_bufferSizeInBytes)
  , m_isMapped(other.m_isMapped)
  , m_mappedLength(other.m_mappedLength)
  , m_requiresExplicitFlush(other.m_requiresExplicitFlush)
{
  other.m_id = 0;
  other.m_bufferSizeInBytes = 0;
  other.m_isMapped = false;
  other.m_mappedLength = 0;
  other.m_requiresExplicitFlush = false;
}

GLBufferObject& GLBufferObject::operator=(GLBufferObject&& other) noexcept
{
  if (this != &other) {
    destroy();

    std::swap(m_id, other.m_id);
    std::swap(m_type, other.m_type);
    std::swap(m_typeEnum, other.m_typeEnum);
    std::swap(m_usagePattern, other.m_usagePattern);
    std::swap(m_bufferSizeInBytes, other.m_bufferSizeInBytes);
    std::swap(m_isMapped, other.m_isMapped);
    std::swap(m_mappedLength, other.m_mappedLength);
    std::swap(m_requiresExplicitFlush, other.m_requiresExplicitFlush);
  }

  return *this;
}

void GLBufferObject::generate()
{
  if (m_id != 0u) {
    return;
  }
  glGenBuffers(1, &m_id);
  CHECK_GL_ERROR(m_errorChecker);
}

void GLBufferObject::destroy()
{
  if (m_id != 0u) {
    glDeleteBuffers(1, &m_id);
  }
  m_id = 0;
  m_bufferSizeInBytes = 0;
  m_isMapped = false;
  m_mappedLength = 0;
  m_requiresExplicitFlush = false;
}

void GLBufferObject::bind() const
{
  if (m_id == 0u) {
    throwDebug("Cannot bind an OpenGL buffer before generating it");
  }
  glBindBuffer(m_typeEnum, m_id);
  CHECK_GL_ERROR(m_errorChecker);
}

void GLBufferObject::unbind() const
{
  glBindBuffer(m_typeEnum, 0);
  CHECK_GL_ERROR(m_errorChecker);
}

void GLBufferObject::allocate(std::size_t sizeInBytes, const GLvoid* data)
{
  if (m_id == 0u) {
    throwDebug("Cannot allocate an OpenGL buffer before generating it");
  }
  if (m_isMapped) {
    throwDebug("Cannot reallocate a mapped OpenGL buffer");
  }

  const GLsizeiptr glSize = checkedBufferSize(sizeInBytes);
  const ScopedBufferBinding binding(m_typeEnum, m_id);

  glBufferData(m_typeEnum, glSize, data, underlyingType(m_usagePattern));

  CHECK_GL_ERROR(m_errorChecker);
  m_bufferSizeInBytes = sizeInBytes;
}

void GLBufferObject::write(std::size_t offset, std::size_t sizeInBytes, const GLvoid* data)
{
  if (m_isMapped) {
    throwDebug("Cannot write a mapped OpenGL buffer");
  }
  validateBufferRange(m_id, m_bufferSizeInBytes, offset, sizeInBytes, data, true, "write");
  const GLintptr glOffset = checkedBufferOffset(offset);
  const GLsizeiptr glSize = checkedBufferSize(sizeInBytes);
  const ScopedBufferBinding binding(m_typeEnum, m_id);

  glBufferSubData(m_typeEnum, glOffset, glSize, data);

  CHECK_GL_ERROR(m_errorChecker);
}

void GLBufferObject::read(std::size_t offset, std::size_t sizeInBytes, GLvoid* data) const
{
  validateBufferRange(m_id, m_bufferSizeInBytes, offset, sizeInBytes, data, true, "read");
  const ScopedBufferBinding binding(m_typeEnum, m_id);
  glGetBufferSubData(m_typeEnum, checkedBufferOffset(offset), checkedBufferSize(sizeInBytes), data);

  CHECK_GL_ERROR(m_errorChecker);
}

void* GLBufferObject::map(const BufferMapAccessPolicy& access)
{
  if (m_id == 0u || m_bufferSizeInBytes == 0u) {
    throwDebug("Cannot map an OpenGL buffer without allocated storage");
  }
  if (m_isMapped) {
    throwDebug("OpenGL buffer is already mapped");
  }
  const ScopedBufferBinding binding(m_typeEnum, m_id);
  void* buffer = glMapBuffer(m_typeEnum, underlyingType(access));
  CHECK_GL_ERROR(m_errorChecker);
  m_isMapped = buffer != nullptr;
  m_mappedLength = m_isMapped ? m_bufferSizeInBytes : 0;
  m_requiresExplicitFlush = false;
  return buffer;
}

void* GLBufferObject::mapRange(
  const std::size_t offset,
  const std::size_t length,
  const std::set<BufferMapRangeAccessFlag>& accessFlags)
{
  if (length == 0u) {
    throwDebug("OpenGL mapped buffer range must have positive length");
  }
  validateBufferRange(m_id, m_bufferSizeInBytes, offset, length, nullptr, false, "map");
  if (m_isMapped) {
    throwDebug("OpenGL buffer is already mapped");
  }
  const bool reads = accessFlags.contains(BufferMapRangeAccessFlag::MapReadBit);
  const bool writes = accessFlags.contains(BufferMapRangeAccessFlag::MapWriteBit);
  if (!reads && !writes) {
    throwDebug("OpenGL mapped buffer range requires read or write access");
  }
  if (
    reads && (accessFlags.contains(BufferMapRangeAccessFlag::InvalidateRangeBit) ||
              accessFlags.contains(BufferMapRangeAccessFlag::InvalidateBufferBit) ||
              accessFlags.contains(BufferMapRangeAccessFlag::UnsynchronizedBit)))
  {
    throwDebug("Read mappings cannot invalidate or use unsynchronized buffer access");
  }
  if (accessFlags.contains(BufferMapRangeAccessFlag::FlushExplicitBit) && !writes) {
    throwDebug("Explicit buffer-map flushing requires write access");
  }
  const GLbitfield access = std::accumulate(
    accessFlags.begin(),
    accessFlags.end(),
    GLbitfield{0u},
    [](const GLbitfield flags, const auto flag) { return flags | underlyingType(flag); });

  const ScopedBufferBinding binding(m_typeEnum, m_id);
  void* buffer = glMapBufferRange(m_typeEnum, checkedBufferOffset(offset), checkedBufferSize(length), access);
  CHECK_GL_ERROR(m_errorChecker);
  m_isMapped = buffer != nullptr;
  m_mappedLength = m_isMapped ? length : 0;
  m_requiresExplicitFlush = m_isMapped && accessFlags.contains(BufferMapRangeAccessFlag::FlushExplicitBit);
  return buffer;
}

void GLBufferObject::flushMappedRange(const std::size_t offset, const std::size_t length)
{
  if (!m_isMapped || !m_requiresExplicitFlush) {
    throwDebug("Buffer mapping does not require explicit flushing");
  }
  if (length == 0u || offset > m_mappedLength || length > m_mappedLength - offset) {
    throwDebug("Flushed buffer range must lie within the mapped range");
  }

  const ScopedBufferBinding binding(m_typeEnum, m_id);
  glFlushMappedBufferRange(m_typeEnum, checkedBufferOffset(offset), checkedBufferSize(length));
  CHECK_GL_ERROR(m_errorChecker);
}

bool GLBufferObject::unmap()
{
  if (!m_isMapped) {
    return false;
  }
  const ScopedBufferBinding binding(m_typeEnum, m_id);
  const bool succeeded = glUnmapBuffer(m_typeEnum) == GL_TRUE;
  CHECK_GL_ERROR(m_errorChecker);
  m_isMapped = false;
  m_mappedLength = 0;
  m_requiresExplicitFlush = false;
  return succeeded;
}

void GLBufferObject::copyData(
  GLBufferObject& readBuffer,
  const GLBufferObject& writeBuffer,
  std::size_t readOffset,
  std::size_t writeOffset,
  std::size_t sizeInBytes)
{
  validateBufferRange(
    readBuffer.m_id,
    readBuffer.m_bufferSizeInBytes,
    readOffset,
    sizeInBytes,
    nullptr,
    false,
    "copy source");
  validateBufferRange(
    writeBuffer.m_id,
    writeBuffer.m_bufferSizeInBytes,
    writeOffset,
    sizeInBytes,
    nullptr,
    false,
    "copy destination");
  if (readBuffer.m_isMapped || writeBuffer.m_isMapped) {
    throwDebug("Cannot copy mapped OpenGL buffers");
  }
  if (&readBuffer == &writeBuffer) {
    const bool overlaps = readOffset < writeOffset + sizeInBytes && writeOffset < readOffset + sizeInBytes;
    if (overlaps && sizeInBytes > 0u) {
      throwDebug("OpenGL buffer copy source and destination ranges overlap");
    }
  }

  const ScopedBufferBinding readBinding(GL_COPY_READ_BUFFER, readBuffer.m_id);
  const ScopedBufferBinding writeBinding(GL_COPY_WRITE_BUFFER, writeBuffer.m_id);

  glCopyBufferSubData(
    GL_COPY_READ_BUFFER,
    GL_COPY_WRITE_BUFFER,
    checkedBufferOffset(readOffset),
    checkedBufferOffset(writeOffset),
    checkedBufferSize(sizeInBytes));

  CHECK_GL_ERROR(readBuffer.m_errorChecker);
}

GLuint GLBufferObject::id() const
{
  return m_id;
}

BufferType GLBufferObject::type() const
{
  return m_type;
}

BufferUsagePattern GLBufferObject::usagePattern() const
{
  return m_usagePattern;
}

size_t GLBufferObject::size() const
{
  return static_cast<size_t>(m_bufferSizeInBytes);
}
