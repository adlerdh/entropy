#include "rendering/gl/GLBufferTexture.h"
#include "rendering/helpers/UnderlyingEnumType.h"

#include "common/Exception.hpp"

#include <glad/glad.h>

#include <sstream>
#include <unordered_map>
#include <utility>

namespace
{

static const std::unordered_map<tex::SizedInternalBufferTextureFormat, uint32_t> sk_textureFormatToNumComponentsMap = {
  {tex::SizedInternalBufferTextureFormat::R8_UNorm, 1},    {tex::SizedInternalBufferTextureFormat::R16_UNorm, 1},
  {tex::SizedInternalBufferTextureFormat::R16F, 1},        {tex::SizedInternalBufferTextureFormat::R32F, 1},
  {tex::SizedInternalBufferTextureFormat::R8I, 1},         {tex::SizedInternalBufferTextureFormat::R16I, 1},
  {tex::SizedInternalBufferTextureFormat::R32I, 1},        {tex::SizedInternalBufferTextureFormat::R8U, 1},
  {tex::SizedInternalBufferTextureFormat::R16U, 1},        {tex::SizedInternalBufferTextureFormat::R32U, 1},

  {tex::SizedInternalBufferTextureFormat::RG8_UNorm, 2},   {tex::SizedInternalBufferTextureFormat::RG16_UNorm, 2},
  {tex::SizedInternalBufferTextureFormat::RG16F, 2},       {tex::SizedInternalBufferTextureFormat::RG32F, 2},
  {tex::SizedInternalBufferTextureFormat::RG8I, 2},        {tex::SizedInternalBufferTextureFormat::RG16I, 2},
  {tex::SizedInternalBufferTextureFormat::RG32I, 2},       {tex::SizedInternalBufferTextureFormat::RG8U, 2},
  {tex::SizedInternalBufferTextureFormat::RG16U, 2},       {tex::SizedInternalBufferTextureFormat::RG32U, 2},

  {tex::SizedInternalBufferTextureFormat::RGB32F, 3},      {tex::SizedInternalBufferTextureFormat::RGB32I, 3},
  {tex::SizedInternalBufferTextureFormat::RGB32UI, 3},

  {tex::SizedInternalBufferTextureFormat::RGBA8_UNorm, 4}, {tex::SizedInternalBufferTextureFormat::RGBA16_UNorm, 4},
  {tex::SizedInternalBufferTextureFormat::RGBA16F, 4},     {tex::SizedInternalBufferTextureFormat::RGBA32F, 4},
  {tex::SizedInternalBufferTextureFormat::RGBA8I, 4},      {tex::SizedInternalBufferTextureFormat::RGBA16I, 4},
  {tex::SizedInternalBufferTextureFormat::RGBA32I, 4},     {tex::SizedInternalBufferTextureFormat::RGBA8U, 4},
  {tex::SizedInternalBufferTextureFormat::RGBA16U, 4},     {tex::SizedInternalBufferTextureFormat::RGBA32U, 4}};

static const std::unordered_map<tex::SizedInternalBufferTextureFormat, uint32_t>
  sk_textureFormatToNumBytesPerComponentMap = {
    {tex::SizedInternalBufferTextureFormat::R8_UNorm, 1},    {tex::SizedInternalBufferTextureFormat::R16_UNorm, 2},
    {tex::SizedInternalBufferTextureFormat::R16F, 2},        {tex::SizedInternalBufferTextureFormat::R32F, 4},
    {tex::SizedInternalBufferTextureFormat::R8I, 1},         {tex::SizedInternalBufferTextureFormat::R16I, 2},
    {tex::SizedInternalBufferTextureFormat::R32I, 4},        {tex::SizedInternalBufferTextureFormat::R8U, 1},
    {tex::SizedInternalBufferTextureFormat::R16U, 2},        {tex::SizedInternalBufferTextureFormat::R32U, 4},

    {tex::SizedInternalBufferTextureFormat::RG8_UNorm, 1},   {tex::SizedInternalBufferTextureFormat::RG16_UNorm, 2},
    {tex::SizedInternalBufferTextureFormat::RG16F, 2},       {tex::SizedInternalBufferTextureFormat::RG32F, 4},
    {tex::SizedInternalBufferTextureFormat::RG8I, 1},        {tex::SizedInternalBufferTextureFormat::RG16I, 2},
    {tex::SizedInternalBufferTextureFormat::RG32I, 4},       {tex::SizedInternalBufferTextureFormat::RG8U, 1},
    {tex::SizedInternalBufferTextureFormat::RG16U, 2},       {tex::SizedInternalBufferTextureFormat::RG32U, 4},

    {tex::SizedInternalBufferTextureFormat::RGB32F, 4},      {tex::SizedInternalBufferTextureFormat::RGB32I, 4},
    {tex::SizedInternalBufferTextureFormat::RGB32UI, 4},

    {tex::SizedInternalBufferTextureFormat::RGBA8_UNorm, 1}, {tex::SizedInternalBufferTextureFormat::RGBA16_UNorm, 2},
    {tex::SizedInternalBufferTextureFormat::RGBA16F, 2},     {tex::SizedInternalBufferTextureFormat::RGBA32F, 4},
    {tex::SizedInternalBufferTextureFormat::RGBA8I, 1},      {tex::SizedInternalBufferTextureFormat::RGBA16I, 2},
    {tex::SizedInternalBufferTextureFormat::RGBA32I, 4},     {tex::SizedInternalBufferTextureFormat::RGBA8U, 1},
    {tex::SizedInternalBufferTextureFormat::RGBA16U, 2},     {tex::SizedInternalBufferTextureFormat::RGBA32U, 4}};

std::size_t bytesPerTexel(const tex::SizedInternalBufferTextureFormat format)
{
  return static_cast<std::size_t>(sk_textureFormatToNumComponentsMap.at(format)) *
         static_cast<std::size_t>(sk_textureFormatToNumBytesPerComponentMap.at(format));
}

std::size_t texelCountForBufferSize(const std::size_t sizeInBytes, const tex::SizedInternalBufferTextureFormat format)
{
  const std::size_t texelSize = bytesPerTexel(format);
  if (sizeInBytes % texelSize != 0u) {
    throwDebug("Texture-buffer storage size must contain a whole number of texels");
  }
  return sizeInBytes / texelSize;
}

} // namespace

GLBufferTexture::GLBufferTexture(const tex::SizedInternalBufferTextureFormat& format, const BufferUsagePattern& usage)
  : m_buffer(BufferType::Texture, usage), m_texture(tex::Target::TextureBuffer), m_format(format)
{
}

GLBufferTexture::GLBufferTexture(GLBufferTexture&& other) noexcept
  : m_buffer(std::move(other.m_buffer)), m_texture(std::move(other.m_texture)), m_format(other.m_format)
{
}

GLBufferTexture& GLBufferTexture::operator=(GLBufferTexture&& other) noexcept
{
  if (this != &other) {
    m_buffer = std::move(other.m_buffer);
    m_texture = std::move(other.m_texture);
    m_format = other.m_format;
  }

  return *this;
}

GLBufferTexture::~GLBufferTexture() = default;

void GLBufferTexture::generate()
{
  m_buffer.generate();
  m_texture.generate();
}

void GLBufferTexture::bind(std::optional<uint32_t> textureUnit) const
{
  m_texture.bind(textureUnit);
}

bool GLBufferTexture::isBound(std::optional<uint32_t> textureUnit) const
{
  return m_texture.isBound(textureUnit);
}

void GLBufferTexture::unbind(std::optional<uint32_t> textureUnit) const
{
  m_texture.unbind(textureUnit);
}

void GLBufferTexture::allocate(std::size_t sizeInBytes, const GLvoid* data)
{
  if (sizeInBytes == 0u) {
    throwDebug("Texture-buffer storage cannot be empty");
  }
  GLint maxSize = 0;
  glGetIntegerv(GL_MAX_TEXTURE_BUFFER_SIZE, &maxSize);
  if (maxSize <= 0) {
    throwDebug("OpenGL reported an invalid maximum texture-buffer size");
  }

  const std::size_t texelCount = texelCountForBufferSize(sizeInBytes, m_format);

  if (texelCount > static_cast<std::size_t>(maxSize)) {
    std::ostringstream ss;
    ss << "Attempting to allocate " << texelCount << " texels (" << sizeInBytes
       << " bytes) in the texel array of a texture buffer object, which is greater than the maximum of " << maxSize
       << " texels" << std::ends;

    throwDebug(ss.str());
  }

  m_buffer.allocate(sizeInBytes, data);
  m_texture.setBufferTextureStorage(underlyingType_asInt32(m_format), m_buffer.id(), texelCount);
}

void GLBufferTexture::write(std::size_t offset, std::size_t sizeInBytes, const GLvoid* data)
{
  m_buffer.write(offset, sizeInBytes, data);
}

void GLBufferTexture::read(std::size_t offset, std::size_t sizeInBytes, GLvoid* data) const
{
  m_buffer.read(offset, sizeInBytes, data);
}

BufferUsagePattern GLBufferTexture::usagePattern() const
{
  return m_buffer.usagePattern();
}

GLuint GLBufferTexture::id() const
{
  return m_texture.id();
}
