#pragma once

#include "rendering/gl/VertexAttributeInfo.h"
#include "rendering/gl/VertexIndicesInfo.h"
#include "rendering/gl/GLBufferTypes.h"
#include "rendering/gl/GLDrawTypes.h"
#include "rendering/gl/GLErrorChecker.h"

#include <glm/vec4.hpp>

#include <glad/glad.h>

#include <cstddef>

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

    explicit IndexedDrawParams(const VertexIndicesInfo& indicesInfo);

    GLenum primitiveMode() const;
    std::size_t elementCount() const;
    GLenum indexType() const;
    const GLvoid* indices() const;

    void setElementCount(std::size_t elementCountArg);

  private:
    GLenum m_primitiveMode;
    std::size_t m_elementCount;
    GLenum m_indexType;
    const GLvoid* m_indices;
  };

  GLVertexArrayObject();
  ~GLVertexArrayObject();

  GLVertexArrayObject(const GLVertexArrayObject&) = delete;
  GLVertexArrayObject& operator=(const GLVertexArrayObject&) = delete;

  GLVertexArrayObject(GLVertexArrayObject&& other) noexcept;
  GLVertexArrayObject& operator=(GLVertexArrayObject&& other) noexcept;

  /// Generate the VAO name if needed.
  void generate();

  /// Delete the VAO name and reset local state.
  void destroy();

  /// Bind this VAO to the current context.
  void bind() const;

  /// Unbind the active VAO from the current context.
  static void unbind();

  GLuint id() const;

  /// Configure a floating-point vertex attribute stream.
  void setAttributeBuffer(
    GLuint index,
    GLint size,
    const BufferComponentType& type,
    const BufferNormalizeValues& normalize,
    GLsizei stride,
    std::size_t offset) const;

  /// Configure a floating-point vertex attribute stream from a stored attribute descriptor.
  void setAttributeBuffer(GLuint index, const VertexAttributeInfo& attribInfo) const;

  /// Configure an integer vertex attribute stream.
  void setAttributeIntegerBuffer(
    GLuint index,
    GLint size,
    const BufferComponentType& type,
    GLsizei stride,
    std::size_t offset) const;

  /// Enable one vertex attribute index.
  void enableVertexAttribute(GLuint index) const;

  /// Disable one vertex attribute index.
  void disableVertexAttribute(GLuint index) const;

  /// Issue `glDrawElements()` with precomputed draw arguments.
  static void drawElements(const IndexedDrawParams& params);

  /// Issue `glDrawArrays()` for an already bound VAO.
  static void drawArrays(const PrimitiveMode& primitiveMode, GLint first, std::size_t count);

private:
  void requireBound() const;

  GLuint m_id;
  GLErrorChecker m_errorChecker;
};
