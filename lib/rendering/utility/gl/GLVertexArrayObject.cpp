#include "rendering/utility/gl/GLVertexArrayObject.h"
#include "rendering/utility/UnderlyingEnumType.h"

#include "common/Exception.hpp"

#include <glad/glad.h>

#include <cstdint>
#include <limits>

namespace
{

size_t bytesPerIndexType(const IndexType& indexType)
{
  switch (indexType) {
    case IndexType::UInt8:
      return 1;
    case IndexType::UInt16:
      return 2;
    case IndexType::UInt32:
      return 4;
  }
  throwDebug("Invalid index type");
}

const GLvoid* indexBufferOffset(const std::size_t indexOffset, const IndexType indexType)
{
  const std::size_t bytesPerIndex = bytesPerIndexType(indexType);
  if (indexOffset > std::numeric_limits<std::uintptr_t>::max() / bytesPerIndex) {
    throwDebug("Index-buffer byte offset exceeds the addressable range");
  }
  return reinterpret_cast<const GLvoid*>(static_cast<std::uintptr_t>(indexOffset * bytesPerIndex));
}

bool isIntegerAttributeType(const BufferComponentType type) noexcept
{
  switch (type) {
    case BufferComponentType::Byte:
    case BufferComponentType::UByte:
    case BufferComponentType::Short:
    case BufferComponentType::UShort:
    case BufferComponentType::Int:
    case BufferComponentType::UInt:
      return true;
    case BufferComponentType::HFloat:
    case BufferComponentType::Float:
    case BufferComponentType::Double:
    case BufferComponentType::Int_2_10_10_10:
    case BufferComponentType::UInt_2_10_10_10:
    case BufferComponentType::UInt_10F_11F_11F:
      return false;
  }
  return false;
}

void validateFloatingAttributeLayout(const GLint size, const BufferComponentType type, const GLsizei stride)
{
  if (size < 1 || size > 4 || stride < 0) {
    throwDebug("Invalid floating-point vertex attribute layout");
  }
  if ((type == BufferComponentType::Int_2_10_10_10 || type == BufferComponentType::UInt_2_10_10_10) && size != 4) {
    throwDebug("2-10-10-10 packed vertex attributes require four components");
  }
  if (type == BufferComponentType::UInt_10F_11F_11F && size != 3) {
    throwDebug("10F-11F-11F packed vertex attributes require three components");
  }
}

} // namespace

GLVertexArrayObject::GLVertexArrayObject() : m_id(0) {}

GLVertexArrayObject::~GLVertexArrayObject()
{
  destroy();
}

GLVertexArrayObject::GLVertexArrayObject(GLVertexArrayObject&& other) noexcept : m_id(other.m_id)
{
  other.m_id = 0;
}

GLVertexArrayObject& GLVertexArrayObject::operator=(GLVertexArrayObject&& other) noexcept
{
  if (this != &other) {
    destroy();
    m_id = other.m_id;
    other.m_id = 0;
  }

  return *this;
}

void GLVertexArrayObject::generate()
{
  if (m_id != 0u) {
    return;
  }
  glGenVertexArrays(1, &m_id);

  CHECK_GL_ERROR(m_errorChecker);
}

void GLVertexArrayObject::destroy()
{
  if (m_id != 0u) {
    glDeleteVertexArrays(1, &m_id);
  }
  m_id = 0;
}

void GLVertexArrayObject::bind() const
{
  if (m_id == 0u) {
    throwDebug("Cannot bind a vertex array before generating it");
  }
  glBindVertexArray(m_id);
  CHECK_GL_ERROR(m_errorChecker);
}

void GLVertexArrayObject::unbind()
{
  glBindVertexArray(0);
  GLErrorChecker{}(__FILE__, __FUNCTION__, __LINE__);
}

GLuint GLVertexArrayObject::id() const
{
  return m_id;
}

void GLVertexArrayObject::setAttributeBuffer(
  GLuint index,
  GLint size,
  const BufferComponentType& type,
  const BufferNormalizeValues& normalize,
  GLsizei stride,
  std::size_t offset) const
{
  requireBound();
  validateFloatingAttributeLayout(size, type, stride);
  glVertexAttribPointer(
    index,
    size,
    underlyingType(type),
    underlyingType(normalize),
    stride,
    reinterpret_cast<const GLvoid*>(static_cast<std::uintptr_t>(offset)));

  CHECK_GL_ERROR(m_errorChecker);
}

void GLVertexArrayObject::setAttributeBuffer(GLuint index, const VertexAttributeInfo& attribInfo) const
{
  setAttributeBuffer(
    index,
    attribInfo.numComponents(),
    attribInfo.componentType(),
    attribInfo.normalizeValues(),
    attribInfo.strideInBytes(),
    attribInfo.offsetInBytes());
}

void GLVertexArrayObject::setAttributeIntegerBuffer(
  GLuint index,
  GLint size,
  const BufferComponentType& type,
  GLsizei stride,
  std::size_t offset) const
{
  requireBound();
  if (size < 1 || size > 4 || stride < 0 || !isIntegerAttributeType(type)) {
    throwDebug("Invalid integer vertex attribute layout");
  }
  glVertexAttribIPointer(
    index,
    size,
    underlyingType(type),
    stride,
    reinterpret_cast<const GLvoid*>(static_cast<std::uintptr_t>(offset)));

  CHECK_GL_ERROR(m_errorChecker);
}

void GLVertexArrayObject::enableVertexAttribute(const GLuint index) const
{
  requireBound();
  glEnableVertexAttribArray(index);
  CHECK_GL_ERROR(m_errorChecker);
}

void GLVertexArrayObject::disableVertexAttribute(const GLuint index) const
{
  requireBound();
  glDisableVertexAttribArray(index);
  CHECK_GL_ERROR(m_errorChecker);
}

// If an attribute is disabled, its value comes from regular OpenGL state.
// Namely, the state set by the glVertexAttrib functions
// void GLVertexArrayObject::setGenericAttribute2f(
//        GLuint index, const glm::vec2& values )
//{
//    glVertexAttrib2f( index, values[0], values[1] );
//}

// void GLVertexArrayObject::setGenericAttribute4f(
//         GLuint index, const glm::vec4& values )
//{
//     glVertexAttrib4f( index, values[0], values[1], values[2], values[3] );
// }

void GLVertexArrayObject::drawElements(const IndexedDrawParams& params)
{
  glDrawElements(
    params.primitiveMode(),
    static_cast<GLsizei>(params.elementCount()),
    params.indexType(),
    params.indices());
}

void GLVertexArrayObject::drawArrays(const PrimitiveMode& primitiveMode, const GLint first, const std::size_t count)
{
  if (first < 0 || count > static_cast<std::size_t>(std::numeric_limits<GLsizei>::max())) {
    throwDebug("Invalid non-indexed draw range");
  }
  glDrawArrays(underlyingType(primitiveMode), first, static_cast<GLsizei>(count));
}

GLVertexArrayObject::IndexedDrawParams::IndexedDrawParams(
  const PrimitiveMode& primitiveMode,
  std::size_t elementCount,
  const IndexType& indexType,
  std::size_t indexOffset)
  : m_primitiveMode(underlyingType(primitiveMode))
  , m_elementCount(0)
  , m_indexType(underlyingType(indexType))
  , m_indices(indexBufferOffset(indexOffset, indexType))
{
  setElementCount(elementCount);
}

GLVertexArrayObject::IndexedDrawParams::IndexedDrawParams(const VertexIndicesInfo& indicesInfo)
  : m_primitiveMode(underlyingType(indicesInfo.primitiveMode()))
  , m_elementCount(0)
  , m_indexType(underlyingType(indicesInfo.indexType()))
  , m_indices(indexBufferOffset(indicesInfo.indexOffset(), indicesInfo.indexType()))
{
  setElementCount(indicesInfo.indexCount());
}

GLenum GLVertexArrayObject::IndexedDrawParams::primitiveMode() const
{
  return m_primitiveMode;
}

size_t GLVertexArrayObject::IndexedDrawParams::elementCount() const
{
  return m_elementCount;
}

void GLVertexArrayObject::IndexedDrawParams::setElementCount(size_t elementCountArg)
{
  if (elementCountArg > std::numeric_limits<GLsizei>::max()) {
    throwDebug("Attempting to set more elements than max count");
  }

  m_elementCount = elementCountArg;
}

GLenum GLVertexArrayObject::IndexedDrawParams::indexType() const
{
  return m_indexType;
}

const GLvoid* GLVertexArrayObject::IndexedDrawParams::indices() const
{
  return m_indices;
}

void GLVertexArrayObject::requireBound() const
{
  if (m_id == 0u) {
    throwDebug("Cannot configure a vertex array before generating it");
  }
  GLint boundVertexArray = 0;
  glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &boundVertexArray);
  if (static_cast<GLuint>(boundVertexArray) != m_id) {
    throwDebug("Vertex array must be bound before configuring its attributes");
  }
}
