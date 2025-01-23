#include "rendering/utility/gl/GLBufferObject.h"
#include "rendering/utility/UnderlyingEnumType.h"

#include "common/Exception.hpp"

#include <limits>

GLBufferObject::GLBufferObject(const BufferType& type, const BufferUsagePattern& usage)
  : m_id(0)
  , m_type(type)
  , m_typeEnum(underlyingType(type))
  , m_usagePattern(usage)
  , m_bufferSizeInBytes(0)
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
{
  other.m_id = 0;
  other.m_bufferSizeInBytes = 0;
}

GLBufferObject& GLBufferObject::operator=(GLBufferObject&& other) noexcept
{
  if (this != &other)
  {
    destroy();

    std::swap(m_id, other.m_id);
    std::swap(m_type, other.m_type);
    std::swap(m_typeEnum, other.m_typeEnum);
    std::swap(m_usagePattern, other.m_usagePattern);
    std::swap(m_bufferSizeInBytes, other.m_bufferSizeInBytes);
  }

  return *this;
}

void GLBufferObject::generate()
{
  glGenBuffers(1, &m_id);
  CHECK_GL_ERROR(m_errorChecker);
}

void GLBufferObject::release()
{
  glBindBuffer(m_typeEnum, 0);
  CHECK_GL_ERROR(m_errorChecker);
}

void GLBufferObject::destroy()
{
  glDeleteBuffers(1, &m_id);
  m_id = 0;
  m_bufferSizeInBytes = 0;
}

void GLBufferObject::bind()
{
  glBindBuffer(m_typeEnum, m_id);
  CHECK_GL_ERROR(m_errorChecker);
}

void GLBufferObject::unbind()
{
  glBindBuffer(m_typeEnum, 0);
  CHECK_GL_ERROR(m_errorChecker);
}

void GLBufferObject::allocate(std::size_t sizeInBytes, const GLvoid* data)
{
  if (sizeInBytes > static_cast<size_t>(std::numeric_limits<GLsizeiptr>::max()))
  {
    throw_debug("Attempting to allocate GLBufferObject larger than maximum size");
  }

  bind();

  glBufferData(m_typeEnum, static_cast<GLsizeiptr>(sizeInBytes), data, underlyingType(m_usagePattern));

  m_bufferSizeInBytes = sizeInBytes;

  unbind();

  CHECK_GL_ERROR(m_errorChecker);
}

void GLBufferObject::write(std::size_t offset, std::size_t sizeInBytes, const GLvoid* data)
{
  if (offset > static_cast<size_t>(std::numeric_limits<GLsizeiptr>::max()) || sizeInBytes > static_cast<size_t>(std::numeric_limits<GLsizeiptr>::max()))
  {
    throw_debug("Attempting to wrote GLBufferObject larger than maximum size");
  }

  bind();
  glBufferSubData(
    m_typeEnum, static_cast<GLsizeiptr>(offset), static_cast<GLsizeiptr>(sizeInBytes), data
  );

  unbind();

  CHECK_GL_ERROR(m_errorChecker);
}

void GLBufferObject::read(std::size_t offset, std::size_t size, GLvoid* data)
{
  glGetBufferSubData(m_typeEnum, static_cast<GLsizeiptr>(offset), static_cast<GLsizeiptr>(size), data);

  CHECK_GL_ERROR(m_errorChecker);
}

void* GLBufferObject::map(const BufferMapAccessPolicy& access)
{
  void* buffer = glMapBuffer(m_typeEnum, underlyingType(access));
  CHECK_GL_ERROR(m_errorChecker);
  return buffer;
}

void* GLBufferObject::mapRange(
  GLintptr offset, GLsizeiptr length, const std::set<BufferMapRangeAccessFlag>& accessFlags
)
{
  GLbitfield access = 0u;
  for (const BufferMapRangeAccessFlag& f : accessFlags)
  {
    access |= underlyingType(f);
  }

  void* buffer = glMapBufferRange(m_typeEnum, offset, length, access);
  CHECK_GL_ERROR(m_errorChecker);
  return buffer;
}

bool GLBufferObject::unmap()
{
  return glUnmapBuffer(m_typeEnum);
}

void GLBufferObject::copyData(
  GLBufferObject& readBuffer,
  GLBufferObject& writeBuffer,
  GLintptr readOffset,
  GLintptr writeOffset,
  GLsizeiptr size
)
{
  readBuffer.bind();
  writeBuffer.bind();

  glCopyBufferSubData(
    underlyingType(readBuffer.type()),
    underlyingType(writeBuffer.type()),
    readOffset,
    writeOffset,
    size
  );
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
