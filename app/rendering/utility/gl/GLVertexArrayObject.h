#pragma once

#include "rendering/utility/containers/VertexAttributeInfo.h"
#include "rendering/utility/containers/VertexIndicesInfo.h"
#include "rendering/utility/gl/GLBufferTypes.h"
#include "rendering/utility/gl/GLDrawTypes.h"
#include "rendering/utility/gl/GLErrorChecker.h"

#include <glm/vec4.hpp>

#include <glad/glad.h>

/**
 * @brief RAII wrapper around an OpenGL vertex array object.
 *
 * The wrapper owns the VAO name and provides helpers for configuring floating-point and integer vertex attributes as
 * well as issuing indexed draw calls from an already bound index buffer.
 */
class GLVertexArrayObject final
{
public:
  /**
   * @brief Fully resolved arguments for `glDrawElements()`.
   */
  class IndexedDrawParams
  {
  public:
    IndexedDrawParams(
      const PrimitiveMode& primitiveMode,
      std::size_t elementCount,
      const IndexType& indexType,
      std::size_t indexOffset);

    IndexedDrawParams(const VertexIndicesInfo& indicesInfo);

    GLenum primitiveMode() const;
    std::size_t elementCount() const;
    GLenum indexType() const;
    GLvoid* indices() const;

    void setElementCount(std::size_t elementCount);

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

  /// Generate the VAO name if needed.
  void generate();

  /// Delete the VAO name and reset local state.
  void destroy();

  /// Bind this VAO to the current context.
  void bind() const;

  /// Unbind the active VAO from the current context.
  void release() const;

  GLuint id() const;

  /// Configure a floating-point vertex attribute stream.
  void setAttributeBuffer(
    GLuint index,
    GLint size,
    const BufferComponentType& type,
    const BufferNormalizeValues& normalize,
    GLsizei stride,
    GLint offset);

  /// Configure a floating-point vertex attribute stream from a stored attribute descriptor.
  void setAttributeBuffer(GLuint index, const VertexAttributeInfo& attribInfo);

  /// Configure an integer vertex attribute stream.
  void
  setAttributeIntegerBuffer(GLuint index, GLint size, const BufferComponentType& type, GLsizei stride, GLint offset);

  /// Enable one vertex attribute index.
  void enableVertexAttribute(GLuint index);

  /// Disable one vertex attribute index.
  void disableVertexAttribute(GLuint index);

  /// Issue `glDrawElements()` with precomputed draw arguments.
  void drawElements(const IndexedDrawParams& params) const;

private:
  GLuint m_id;
  GLErrorChecker m_errorChecker;
};
