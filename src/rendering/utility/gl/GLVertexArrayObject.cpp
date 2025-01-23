#include "rendering/utility/gl/GLVertexArrayObject.h"
#include "rendering/utility/UnderlyingEnumType.h"

#include "common/Exception.hpp"

namespace
{

size_t bytesPerIndexType(const IndexType& indexType)
{
  switch (indexType)
  {
  case IndexType::UInt8:
    return 1;
  case IndexType::UInt16:
    return 2;
  case IndexType::UInt32:
    return 4;
  }
  throw_debug("Invalid index type")
}

} // namespace

GLVertexArrayObject::GLVertexArrayObject()
  : m_id(0)
{
}

GLVertexArrayObject::~GLVertexArrayObject()
{
  destroy();
}

void GLVertexArrayObject::generate()
{
  glGenVertexArrays(1, &m_id);

  CHECK_GL_ERROR(m_errorChecker)
}

void GLVertexArrayObject::destroy()
{
  glDeleteVertexArrays(1, &m_id);
}

void GLVertexArrayObject::bind()
{
  glBindVertexArray(m_id);
  CHECK_GL_ERROR(m_errorChecker)
}

void GLVertexArrayObject::release()
{
  glBindVertexArray(0);
  CHECK_GL_ERROR(m_errorChecker)
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
  GLint offset
)
{
  glVertexAttribPointer(
    index,
    size,
    underlyingType(type),
    underlyingType(normalize),
    stride,
    reinterpret_cast<const GLvoid*>(offset)
  );

  CHECK_GL_ERROR(m_errorChecker)
}

void GLVertexArrayObject::setAttributeBuffer(GLuint index, const VertexAttributeInfo& attribInfo)
{
  setAttributeBuffer(
    index,
    attribInfo.numComponents(),
    attribInfo.componentType(),
    attribInfo.normalizeValues(),
    attribInfo.strideInBytes(),
    attribInfo.offsetInBytes()
  );
}

void GLVertexArrayObject::setAttributeIntegerBuffer(
  GLuint index, GLint size, const BufferComponentType& type, GLsizei stride, GLint offset
)
{
  glVertexAttribIPointer(
    index, size, underlyingType(type), stride, reinterpret_cast<const GLvoid*>(offset)
  );

  CHECK_GL_ERROR(m_errorChecker)
}

void GLVertexArrayObject::enableVertexAttribute(GLuint index)
{
  glEnableVertexAttribArray(index);
}

void GLVertexArrayObject::disableVertexAttribute(GLuint index)
{
  glDisableVertexAttribArray(index);
}

// If an attribute is disabled, its value comes from regular OpenGL state.
// Namely, the state set by the glVertexAttrib functions
//void GLVertexArrayObject::setGenericAttribute2f(
//        GLuint index, const glm::vec2& values )
//{
//    glVertexAttrib2f( index, values[0], values[1] );
//}

//void GLVertexArrayObject::setGenericAttribute4f(
//        GLuint index, const glm::vec4& values )
//{
//    glVertexAttrib4f( index, values[0], values[1], values[2], values[3] );
//}

void GLVertexArrayObject::drawElements(const IndexedDrawParams& params)
{
  glDrawElements(
    params.primitiveMode(),
    static_cast<GLsizei>(params.elementCount()),
    params.indexType(),
    params.indices()
  );
}

GLVertexArrayObject::IndexedDrawParams::IndexedDrawParams(
  const PrimitiveMode& primitiveMode,
  std::size_t elementCount,
  const IndexType& indexType,
  std::size_t indexOffset
)
  : m_primitiveMode(underlyingType(primitiveMode))
  , m_elementCount(0)
  , m_indexType(underlyingType(indexType))
  , m_indices(reinterpret_cast<GLvoid*>(indexOffset * bytesPerIndexType(indexType)))
{
  setElementCount(elementCount);
}

GLVertexArrayObject::IndexedDrawParams::IndexedDrawParams(const VertexIndicesInfo& indicesInfo)
  : m_primitiveMode(underlyingType(indicesInfo.primitiveMode()))
  , m_elementCount(indicesInfo.indexCount())
  , m_indexType(underlyingType(indicesInfo.indexType()))
  ,

  m_indices(reinterpret_cast<GLvoid*>(
    indicesInfo.indexOffset() * bytesPerIndexType(indicesInfo.indexType())
  ))
{
}

GLenum GLVertexArrayObject::IndexedDrawParams::primitiveMode() const
{
  return m_primitiveMode;
}

size_t GLVertexArrayObject::IndexedDrawParams::elementCount() const
{
  return m_elementCount;
}

void GLVertexArrayObject::IndexedDrawParams::setElementCount(size_t c)
{
  if (c > std::numeric_limits<GLsizei>::max())
  {
    throw_debug("Attempting to set more elements than max count")
  }

  m_elementCount = c;
}

GLenum GLVertexArrayObject::IndexedDrawParams::indexType() const
{
  return m_indexType;
}

GLvoid* GLVertexArrayObject::IndexedDrawParams::indices() const
{
  return m_indices;
}
