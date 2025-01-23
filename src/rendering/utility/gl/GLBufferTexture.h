#ifndef GLBUFFERTEXTURE
#define GLBUFFERTEXTURE

#include "rendering/utility/gl/GLBufferObject.h"
#include "rendering/utility/gl/GLErrorChecker.h"
#include "rendering/utility/gl/GLTexture.h"

#include <glad/glad.h>

#include <optional>

/**
 * A Buffer Texture is a one-dimensional Texture whose storage comes from a Buffer Object.
 * It is used to allow a shader to access a large table of memory that is managed by a buffer object.
 *
 * @see https://www.khronos.org/opengl/wiki/Buffer_Texture
 */
class GLBufferTexture final
{
public:
  GLBufferTexture(
    const tex::SizedInternalBufferTextureFormat& format, const BufferUsagePattern& usage
  );

  GLBufferTexture(const GLBufferTexture&) = delete;
  GLBufferTexture& operator=(const GLBufferTexture&) = delete;

  GLBufferTexture(GLBufferTexture&&) noexcept;
  GLBufferTexture& operator=(GLBufferTexture&&) noexcept;

  ~GLBufferTexture();

  void generate();
  void release(std::optional<uint32_t> textureUnit = std::nullopt);
  void bind(std::optional<uint32_t> textureUnit = std::nullopt);
  bool isBound(std::optional<uint32_t> textureUnit = std::nullopt);
  void unbind();

  /**
     * @return Texture ID
     */
  GLuint id() const;

  // Allocate buffer
  void allocate(std::size_t sizeInBytes, const GLvoid* data);

  // Write to buffer
  void write(GLintptr offset, GLsizeiptr sizeInBytes, const GLvoid* data);

  void read(GLintptr offset, GLsizeiptr sizeInBytes, GLvoid* data);

  BufferUsagePattern usagePattern() const;

  /**
     * @note When a buffer texture is accessed in a shader, the results of a texel fetch are undefined
     * if the specified texel coordinate is negative, or greater than or equal to the clamped number of
     * texels in the texel array.
     *
     * @return Number of texels in the buffer texture's texel array
     */
  std::size_t numBytes() const;

  /**
     * @brief Attach buffer object's data store to a buffer texture object.
     */
  void attachBufferToTexture(std::optional<uint32_t> textureUnit = std::nullopt);

  void detatchBufferFromTexture();

private:
  GLErrorChecker m_errorChecker;

  GLBufferObject m_buffer;

  // Texture "wrapper" around buffer object: must be a buffer texture
  GLTexture m_texture;

  // Storage format for the texture image found found in the buffer object
  tex::SizedInternalBufferTextureFormat m_format;
};

#endif // GLBUFFERTEXTURE
