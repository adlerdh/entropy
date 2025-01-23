#include "rendering/utility/gl/GLBufferTexture.h"
#include "rendering/utility/UnderlyingEnumType.h"

#include "common/Exception.hpp"

#include <sstream>
#include <unordered_map>

namespace
{

static const std::unordered_map<tex::SizedInternalBufferTextureFormat, uint32_t>
  sk_textureFormatToNumComponentsMap = {
    {tex::SizedInternalBufferTextureFormat::R8_UNorm, 1},
    {tex::SizedInternalBufferTextureFormat::R16_UNorm, 1},
    {tex::SizedInternalBufferTextureFormat::R16F, 1},
    {tex::SizedInternalBufferTextureFormat::R32F, 1},
    {tex::SizedInternalBufferTextureFormat::R8I, 1},
    {tex::SizedInternalBufferTextureFormat::R16I, 1},
    {tex::SizedInternalBufferTextureFormat::R32I, 1},
    {tex::SizedInternalBufferTextureFormat::R8U, 1},
    {tex::SizedInternalBufferTextureFormat::R16U, 1},
    {tex::SizedInternalBufferTextureFormat::R32U, 1},

    {tex::SizedInternalBufferTextureFormat::RG8_UNorm, 2},
    {tex::SizedInternalBufferTextureFormat::RG16_UNorm, 2},
    {tex::SizedInternalBufferTextureFormat::RG16F, 2},
    {tex::SizedInternalBufferTextureFormat::RG32F, 2},
    {tex::SizedInternalBufferTextureFormat::RG8I, 2},
    {tex::SizedInternalBufferTextureFormat::RG16I, 2},
    {tex::SizedInternalBufferTextureFormat::RG32I, 2},
    {tex::SizedInternalBufferTextureFormat::RG8U, 2},
    {tex::SizedInternalBufferTextureFormat::RG16U, 2},
    {tex::SizedInternalBufferTextureFormat::RG32U, 2},

    {tex::SizedInternalBufferTextureFormat::RGB32F, 3},
    {tex::SizedInternalBufferTextureFormat::RGB32I, 3},
    {tex::SizedInternalBufferTextureFormat::RGB32UI, 3},

    {tex::SizedInternalBufferTextureFormat::RGBA8_UNorm, 4},
    {tex::SizedInternalBufferTextureFormat::RGBA16_UNorm, 4},
    {tex::SizedInternalBufferTextureFormat::RGBA16F, 4},
    {tex::SizedInternalBufferTextureFormat::RGBA32F, 4},
    {tex::SizedInternalBufferTextureFormat::RGBA8I, 4},
    {tex::SizedInternalBufferTextureFormat::RGBA16I, 4},
    {tex::SizedInternalBufferTextureFormat::RGBA32I, 4},
    {tex::SizedInternalBufferTextureFormat::RGBA8U, 4},
    {tex::SizedInternalBufferTextureFormat::RGBA16U, 4},
    {tex::SizedInternalBufferTextureFormat::RGBA32U, 4}
};

static const std::unordered_map<tex::SizedInternalBufferTextureFormat, uint32_t>
  sk_textureFormatToNumBytesPerComponentMap = {
    {tex::SizedInternalBufferTextureFormat::R8_UNorm, 1},
    {tex::SizedInternalBufferTextureFormat::R16_UNorm, 2},
    {tex::SizedInternalBufferTextureFormat::R16F, 2},
    {tex::SizedInternalBufferTextureFormat::R32F, 4},
    {tex::SizedInternalBufferTextureFormat::R8I, 1},
    {tex::SizedInternalBufferTextureFormat::R16I, 2},
    {tex::SizedInternalBufferTextureFormat::R32I, 4},
    {tex::SizedInternalBufferTextureFormat::R8U, 1},
    {tex::SizedInternalBufferTextureFormat::R16U, 2},
    {tex::SizedInternalBufferTextureFormat::R32U, 4},

    {tex::SizedInternalBufferTextureFormat::RG8_UNorm, 1},
    {tex::SizedInternalBufferTextureFormat::RG16_UNorm, 2},
    {tex::SizedInternalBufferTextureFormat::RG16F, 2},
    {tex::SizedInternalBufferTextureFormat::RG32F, 4},
    {tex::SizedInternalBufferTextureFormat::RG8I, 1},
    {tex::SizedInternalBufferTextureFormat::RG16I, 2},
    {tex::SizedInternalBufferTextureFormat::RG32I, 4},
    {tex::SizedInternalBufferTextureFormat::RG8U, 1},
    {tex::SizedInternalBufferTextureFormat::RG16U, 2},
    {tex::SizedInternalBufferTextureFormat::RG32U, 4},

    {tex::SizedInternalBufferTextureFormat::RGB32F, 4},
    {tex::SizedInternalBufferTextureFormat::RGB32I, 4},
    {tex::SizedInternalBufferTextureFormat::RGB32UI, 4},

    {tex::SizedInternalBufferTextureFormat::RGBA8_UNorm, 1},
    {tex::SizedInternalBufferTextureFormat::RGBA16_UNorm, 2},
    {tex::SizedInternalBufferTextureFormat::RGBA16F, 2},
    {tex::SizedInternalBufferTextureFormat::RGBA32F, 4},
    {tex::SizedInternalBufferTextureFormat::RGBA8I, 1},
    {tex::SizedInternalBufferTextureFormat::RGBA16I, 2},
    {tex::SizedInternalBufferTextureFormat::RGBA32I, 4},
    {tex::SizedInternalBufferTextureFormat::RGBA8U, 1},
    {tex::SizedInternalBufferTextureFormat::RGBA16U, 2},
    {tex::SizedInternalBufferTextureFormat::RGBA32U, 4}
};

} // namespace

GLBufferTexture::GLBufferTexture(
  const tex::SizedInternalBufferTextureFormat& format, const BufferUsagePattern& usage
)
  : m_buffer(BufferType::Texture, usage)
  , m_texture(tex::Target::TextureBuffer)
  , m_format(format)
{
}

GLBufferTexture::GLBufferTexture(GLBufferTexture&& other) noexcept
  : m_buffer(std::move(other.m_buffer))
  , m_texture(std::move(other.m_texture))
  , m_format(std::move(other.m_format))
{
}

GLBufferTexture& GLBufferTexture::operator=(GLBufferTexture&& other) noexcept
{
  if (this != &other)
  {
    detatchBufferFromTexture();

    m_buffer = std::move(other.m_buffer);
    m_texture = std::move(other.m_texture);
    m_format = std::move(other.m_format);
  }

  return *this;
}

GLBufferTexture::~GLBufferTexture()
{
  detatchBufferFromTexture();
}

void GLBufferTexture::generate()
{
  m_buffer.generate();
  m_texture.generate();
}

void GLBufferTexture::release(std::optional<uint32_t> textureUnit)
{
  m_texture.release(textureUnit);
}

void GLBufferTexture::bind(std::optional<uint32_t> textureUnit)
{
  m_texture.bind(textureUnit);
}

bool GLBufferTexture::isBound(std::optional<uint32_t> textureUnit)
{
  return m_texture.isBound(textureUnit);
}

void GLBufferTexture::unbind()
{
  m_texture.unbind();
}

void GLBufferTexture::allocate(std::size_t sizeInBytes, const GLvoid* data)
{
  GLint maxSize;
  glGetIntegerv(GL_MAX_TEXTURE_BUFFER_SIZE, &maxSize);

  if (sizeInBytes > static_cast<std::size_t>(maxSize))
  {
    std::ostringstream ss;
    ss << "Attempting to allocate " << sizeInBytes
       << " bytes in the texel array of a texture buffer object,"
       << " which is greater than the maximum of " << maxSize << std::ends;

    throw_debug(ss.str());
  }

  m_buffer.allocate(sizeInBytes, data);
}

void GLBufferTexture::write(GLintptr offset, GLsizeiptr sizeInBytes, const GLvoid* data)
{
  m_buffer.write(offset, sizeInBytes, data);
}

void GLBufferTexture::read(GLintptr offset, GLsizeiptr sizeInBytes, GLvoid* data)
{
  m_buffer.read(offset, sizeInBytes, data);
}

BufferUsagePattern GLBufferTexture::usagePattern() const
{
  return m_buffer.usagePattern();
}

// size_t GLBufferTexture::numTexels() const
// {
//     return ( m_buffer.size() *
//              sk_textureFormatToNumComponentsMap.at( m_format ) *
//              sk_textureFormatToNumBytesPerComponentMap.at( m_format ) );
// }

size_t GLBufferTexture::numBytes() const
{
  return m_buffer.size();
}

GLuint GLBufferTexture::id() const
{
  return m_texture.id();
}

void GLBufferTexture::attachBufferToTexture(std::optional<uint32_t> textureUnit)
{
  m_texture.bind(textureUnit);

  glTexBuffer(GL_TEXTURE_BUFFER, underlyingType(m_format), m_buffer.id());
  m_buffer.unbind();

  CHECK_GL_ERROR(m_errorChecker);
}

void GLBufferTexture::detatchBufferFromTexture()
{
  glTexBuffer(GL_TEXTURE_BUFFER, 0, 0);
}
