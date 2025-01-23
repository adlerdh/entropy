#ifndef GL_VERTEX_ARRAY_OBJECT_H
#define GL_VERTEX_ARRAY_OBJECT_H

#include "rendering/utility/containers/VertexAttributeInfo.h"
#include "rendering/utility/containers/VertexIndicesInfo.h"
#include "rendering/utility/gl/GLBufferTypes.h"
#include "rendering/utility/gl/GLDrawTypes.h"
#include "rendering/utility/gl/GLErrorChecker.h"

#include <glm/vec4.hpp>

#include <glad/glad.h>

#include <string>
#include <unordered_map>

class GLVertexArrayObject final
{
public:
  class IndexedDrawParams
  {
  public:
    IndexedDrawParams(
      const PrimitiveMode& primitiveMode,
      std::size_t elementCount,
      const IndexType& indexType,
      std::size_t indexOffset
    );

    IndexedDrawParams(const VertexIndicesInfo& indicesInfo);

    GLenum primitiveMode() const;
    std::size_t elementCount() const;
    GLenum indexType() const;
    GLvoid* indices() const;

    void setElementCount(std::size_t c);

  private:
    GLenum m_primitiveMode;
    std::size_t m_elementCount;
    GLenum m_indexType;
    GLvoid* m_indices;
  };

  GLVertexArrayObject();
  ~GLVertexArrayObject();

  GLVertexArrayObject(const GLVertexArrayObject&) = delete;
  GLVertexArrayObject& operator=(const GLVertexArrayObject&) = delete;

  GLVertexArrayObject(GLVertexArrayObject&&) = default;
  GLVertexArrayObject& operator=(GLVertexArrayObject&&) = default;

  void generate();
  void destroy();

  void bind();
  void release();

  GLuint id() const;

  void setAttributeBuffer(
    GLuint index,
    GLint size,
    const BufferComponentType& type,
    const BufferNormalizeValues& normalize,
    GLsizei stride,
    GLint offset
  );

  void setAttributeBuffer(GLuint index, const VertexAttributeInfo& attribInfo);

  void setAttributeIntegerBuffer(
    GLuint index, GLint size, const BufferComponentType& type, GLsizei stride, GLint offset
  );

  /// @note There is a bug in Qt:
  /// glVertexAttrib family of functions are not defined in the Core profile functions:
  /// @see https://stackoverflow.com/questions/24595609/why-does-qt-consider-glvertexattrib-methods-as-deprecated-compatibility-profile

  // This sets global context state: nothing specific to the VAO
  //    void setGenericAttribute2f( GLuint index, const glm::vec2& values );
  //    void setGenericAttribute4f( GLuint index, const glm::vec4& values );

  void enableVertexAttribute(GLuint index);
  void disableVertexAttribute(GLuint index);

  void drawElements(const IndexedDrawParams& params);

private:
  GLuint m_id;

  GLErrorChecker m_errorChecker;
};

#endif // GL_VERTEX_ARRAY_OBJECT_H
