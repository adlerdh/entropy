#pragma once

#include "rendering/utility/gl/GLBufferObject.h"
#include "rendering/utility/gl/GLErrorChecker.h"
#include "rendering/utility/gl/GLTexture.h"

#include <glad/glad.h>

#include <optional>

/**
 * @brief One-dimensional texture whose storage is backed by an OpenGL buffer object.
 *
 * Buffer textures are used for large shader lookup tables, such as segmentation label color tables, where the GPU
 * should fetch records from buffer storage through a samplerBuffer.
 *
 * @see https://www.khronos.org/opengl/wiki/Buffer_Texture
 */
class GLBufferTexture final
{
public:
  /**
   * @param format Sized format used to interpret texels in the buffer.
   * @param usage Buffer usage hint for the underlying storage.
   */
  GLBufferTexture(const tex::SizedInternalBufferTextureFormat& format, const BufferUsagePattern& usage);

  GLBufferTexture(const GLBufferTexture&) = delete;
  GLBufferTexture& operator=(const GLBufferTexture&) = delete;

  GLBufferTexture(GLBufferTexture&&) noexcept;
  GLBufferTexture& operator=(GLBufferTexture&&) noexcept;

  ~GLBufferTexture();

  /// Generate both the backing buffer object and texture object.
  void generate();

  /// Unbind the texture from the current context or from a specific texture unit.
  void release(std::optional<uint32_t> textureUnit = std::nullopt);

  /// Bind the texture to the current context or to a specific texture unit.
  void bind(std::optional<uint32_t> textureUnit = std::nullopt);

  /// Return whether the texture object is bound, optionally on the supplied texture unit.
  bool isBound(std::optional<uint32_t> textureUnit = std::nullopt);

  /// Unbind the texture target from the current context.
  void unbind();

  /// Return the OpenGL texture name.
  GLuint id() const;

  /// Allocate backing buffer storage and optionally initialize it with CPU data.
  void allocate(std::size_t sizeInBytes, const GLvoid* data);

  /// Replace a byte range in the backing buffer.
  void write(GLintptr offset, GLsizeiptr sizeInBytes, const GLvoid* data);

  /// Read a byte range from the backing buffer.
  void read(GLintptr offset, GLsizeiptr sizeInBytes, GLvoid* data);

  BufferUsagePattern usagePattern() const;

  /**
   * @note When a buffer texture is accessed in a shader, the results of a texel fetch are undefined
   * if the specified texel coordinate is negative, or greater than or equal to the clamped number
   * of texels in the texel array.
   *
   * @return Number of texels in the buffer texture's texel array
   */
  std::size_t numBytes() const;

  /// Attach the backing buffer object's data store to the texture object.
  void attachBufferToTexture(std::optional<uint32_t> textureUnit = std::nullopt);

  /// Detach any buffer storage currently attached to the texture object.
  void detachBufferFromTexture();

  /// @deprecated Use `detachBufferFromTexture()`.
  void detatchBufferFromTexture();

private:
  GLErrorChecker m_errorChecker;

  GLBufferObject m_buffer;

  // Texture "wrapper" around buffer object: must be a buffer texture
  GLTexture m_texture;

  // Storage format for the texture image found found in the buffer object
  tex::SizedInternalBufferTextureFormat m_format;
};
