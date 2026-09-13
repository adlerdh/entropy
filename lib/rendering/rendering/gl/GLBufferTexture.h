#pragma once

#include "rendering/gl/GLBufferObject.h"
#include "rendering/gl/GLTexture.h"

#include <glad/glad.h>

#include <cstddef>
#include <cstdint>
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

  GLBufferTexture(GLBufferTexture&& other) noexcept;
  GLBufferTexture& operator=(GLBufferTexture&& other) noexcept;

  ~GLBufferTexture();

  /// Generate both the backing buffer object and texture object.
  void generate();

  /// Bind the texture to the current context or to a specific texture unit.
  void bind(std::optional<uint32_t> textureUnit = std::nullopt) const;

  /// Return whether the texture object is bound, optionally on the supplied texture unit.
  bool isBound(std::optional<uint32_t> textureUnit = std::nullopt) const;

  /// Unbind the texture target from the current context or from a specific texture unit.
  void unbind(std::optional<uint32_t> textureUnit = std::nullopt) const;

  /// Return the OpenGL texture name.
  GLuint id() const;

  /// Allocate backing storage, optionally initialize it, and attach it to the texture object.
  void allocate(std::size_t sizeInBytes, const GLvoid* data);

  /// Replace a byte range in the backing buffer.
  void write(std::size_t offset, std::size_t sizeInBytes, const GLvoid* data);

  /// Read a byte range from the backing buffer.
  void read(std::size_t offset, std::size_t sizeInBytes, GLvoid* data) const;

  BufferUsagePattern usagePattern() const;

private:
  GLBufferObject m_buffer;

  // Texture "wrapper" around buffer object: must be a buffer texture
  GLTexture m_texture;

  // Storage format for the texture image found found in the buffer object
  tex::SizedInternalBufferTextureFormat m_format;
};
