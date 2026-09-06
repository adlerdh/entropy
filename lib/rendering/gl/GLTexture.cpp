#include "rendering/gl/GLTexture.h"
#include "rendering/helpers/UnderlyingEnumType.h"

#include "common/Exception.hpp"

#include <glad/glad.h>

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/string_cast.hpp>

#include <spdlog/spdlog.h>

#include <algorithm>
#include <limits>
#include <optional>
#include <sstream>
#include <string>
#include <utility>

using namespace tex;

namespace
{

struct TextureLimits
{
  GLint maxTextureSize{0}; // GL_MAX_TEXTURE_SIZE: 1D and 2D max width/height
  GLint maxRectangleTextureSize{0};
  GLint max3DTextureSize{0};      // GL_MAX_3D_TEXTURE_SIZE: 3D max width/height/depth
  GLint maxArrayTextureLayers{0}; // GL_MAX_ARRAY_TEXTURE_LAYERS: array texture layer count
  GLint maxCubeMapTextureSize{0};
};

TextureLimits queryTextureLimits()
{
  TextureLimits limits;
  glGetIntegerv(GL_MAX_TEXTURE_SIZE, &limits.maxTextureSize);
  glGetIntegerv(GL_MAX_RECTANGLE_TEXTURE_SIZE, &limits.maxRectangleTextureSize);
  glGetIntegerv(GL_MAX_3D_TEXTURE_SIZE, &limits.max3DTextureSize);
  glGetIntegerv(GL_MAX_ARRAY_TEXTURE_LAYERS, &limits.maxArrayTextureLayers);
  glGetIntegerv(GL_MAX_CUBE_MAP_TEXTURE_SIZE, &limits.maxCubeMapTextureSize);
  return limits;
}

bool textureTargetSupportsMipmaps(Target target) noexcept
{
  return target != Target::Texture2DMultisample && target != Target::TextureRectangle &&
         target != Target::Texture2DMultisampleArray && target != Target::TextureBuffer;
}

GLint mipmapMaxLevelForTarget(const Target target, const glm::ivec3& size) noexcept
{
  GLint maxDimension = 1;
  switch (target) {
    case Target::Texture1D:
    case Target::Texture1DArray:
      maxDimension = std::max(size.x, 1);
      break;
    case Target::Texture2D:
    case Target::Texture2DArray:
    case Target::TextureCubeMap:
      maxDimension = std::max({size.x, size.y, 1});
      break;
    case Target::Texture3D:
      maxDimension = std::max({size.x, size.y, size.z, 1});
      break;
    case Target::Texture2DMultisample:
    case Target::TextureRectangle:
    case Target::Texture2DMultisampleArray:
    case Target::TextureBuffer:
      return 0;
  }
  return static_cast<GLint>(std::floor(std::log2(static_cast<double>(maxDimension))));
}

glm::ivec3 textureSizeForLevel(const Target target, const glm::uvec3& baseSize, const GLint level)
{
  if (level < 0) {
    throwDebug("Texture mipmap level cannot be negative");
  }
  if (
    (target == Target::Texture2DMultisample || target == Target::Texture2DMultisampleArray ||
     target == Target::TextureRectangle || target == Target::TextureBuffer) &&
    level != 0)
  {
    throwDebug("Texture target only supports mipmap level zero");
  }
  if (level > mipmapMaxLevelForTarget(target, glm::ivec3{baseSize})) {
    throwDebug("Texture mipmap level exceeds the range supported by its base dimensions");
  }

  const auto reduced = [level](const uint32_t dimension) {
    const uint32_t shift = std::min<uint32_t>(static_cast<uint32_t>(level), 31u);
    return static_cast<GLint>(std::max(dimension >> shift, 1u));
  };

  glm::ivec3 size{baseSize};
  switch (target) {
    case Target::Texture1D:
      return {reduced(baseSize.x), 1, 1};
    case Target::Texture2D:
    case Target::TextureRectangle:
    case Target::Texture2DMultisample:
    case Target::TextureCubeMap:
      return {reduced(baseSize.x), reduced(baseSize.y), 1};
    case Target::Texture3D:
      return {reduced(baseSize.x), reduced(baseSize.y), reduced(baseSize.z)};
    case Target::Texture1DArray:
      return {reduced(baseSize.x), static_cast<GLint>(baseSize.y), 1};
    case Target::Texture2DArray:
    case Target::Texture2DMultisampleArray:
      return {reduced(baseSize.x), reduced(baseSize.y), static_cast<GLint>(baseSize.z)};
    case Target::TextureBuffer:
      return size;
  }
  return size;
}

bool isMipmapFilter(const MinificationFilter filter) noexcept
{
  return filter != MinificationFilter::Nearest && filter != MinificationFilter::Linear;
}

bool supportsSamplingParameters(const Target target) noexcept
{
  return target != Target::Texture2DMultisample && target != Target::Texture2DMultisampleArray &&
         target != Target::TextureBuffer;
}

void validatePixelStoreSettings(const GLTexture::PixelStoreSettings& settings)
{
  if (settings.m_alignment != 1 && settings.m_alignment != 2 && settings.m_alignment != 4 && settings.m_alignment != 8)
  {
    throwDebug("Pixel storage alignment must be 1, 2, 4, or 8 bytes");
  }
  if (
    settings.m_skipImages < 0 || settings.m_skipRows < 0 || settings.m_skipPixels < 0 || settings.m_imageHeight < 0 ||
    settings.m_rowLength < 0)
  {
    throwDebug("Pixel storage dimensions and offsets cannot be negative");
  }
}

void validateTextureDimensions(const Target target, const glm::uvec3& size)
{
  switch (target) {
    case Target::Texture1D:
      if (size.y != 1u || size.z != 1u) {
        throwDebug("One-dimensional textures require singleton y and z dimensions");
      }
      break;
    case Target::Texture1DArray:
      if (size.z != 1u) {
        throwDebug("One-dimensional array textures require a singleton z dimension");
      }
      break;
    case Target::Texture2D:
    case Target::TextureRectangle:
    case Target::Texture2DMultisample:
      if (size.z != 1u) {
        throwDebug("Two-dimensional textures require a singleton z dimension");
      }
      break;
    case Target::TextureCubeMap:
      if (size.z != 1u || size.x != size.y) {
        throwDebug("Cube-map textures require square faces and a singleton z dimension");
      }
      break;
    case Target::Texture3D:
    case Target::Texture2DArray:
    case Target::Texture2DMultisampleArray:
      break;
    case Target::TextureBuffer:
      throwDebug("Buffer-texture size is defined by its attached buffer storage");
  }
}

void setTextureLevelRange(GLenum target, GLint maxLevel)
{
  glTexParameteri(target, GL_TEXTURE_BASE_LEVEL, 0);
  glTexParameteri(target, GL_TEXTURE_MAX_LEVEL, maxLevel);
}

const char* textureTargetName(Target target) noexcept
{
  switch (target) {
    case Target::Texture1D:
      return "Texture1D";
    case Target::Texture2D:
      return "Texture2D";
    case Target::Texture3D:
      return "Texture3D";
    case Target::Texture1DArray:
      return "Texture1DArray";
    case Target::Texture2DArray:
      return "Texture2DArray";
    case Target::Texture2DMultisample:
      return "Texture2DMultisample";
    case Target::TextureRectangle:
      return "TextureRectangle";
    case Target::Texture2DMultisampleArray:
      return "Texture2DMultisampleArray";
    case Target::TextureCubeMap:
      return "TextureCubeMap";
    case Target::TextureBuffer:
      return "TextureBuffer";
  }

  return "Unknown";
}

void labelTextureObject(GLuint textureId, Target target, const glm::uvec3& size)
{
  if (textureId == 0 || !GLAD_GL_KHR_debug || glObjectLabel == nullptr) {
    return;
  }

  const std::string label =
    "Entropy texture " + std::to_string(textureId) + " " + textureTargetName(target) + " " + glm::to_string(size);
  glObjectLabel(GL_TEXTURE, textureId, static_cast<GLsizei>(label.size()), label.c_str());
}

uint8_t cubeMapFaceBit(const CubeMapFace face)
{
  switch (face) {
    case CubeMapFace::PositiveX:
      return 1u << 0u;
    case CubeMapFace::NegativeX:
      return 1u << 1u;
    case CubeMapFace::PositiveY:
      return 1u << 2u;
    case CubeMapFace::NegativeY:
      return 1u << 3u;
    case CubeMapFace::PositiveZ:
      return 1u << 4u;
    case CubeMapFace::NegativeZ:
      return 1u << 5u;
  }
  throwDebug("Invalid cube-map face");
}

const char* openGLErrorMessage(GLenum error)
{
  switch (error) {
    case GL_INVALID_ENUM:
      return "Enumeration parameter not legal for function";
    case GL_INVALID_VALUE:
      return "Value parameter not legal for function";
    case GL_INVALID_OPERATION:
      return "Set of state not legal for parameters given to command";
    case GL_OUT_OF_MEMORY:
      return "Memory cannot be allocated for operation";
    case GL_INVALID_FRAMEBUFFER_OPERATION:
      return "Attempt to read from or write/render to incomplete framebuffer";
    default:
      return "Unknown OpenGL error";
  }
}

void throwIfOpenGLErrorAfterTextureUpload(
  GLenum target,
  GLint internalFormat,
  const glm::ivec3& size,
  GLenum format,
  GLenum type)
{
  const GLenum firstError = glGetError();
  if (firstError == GL_NO_ERROR) {
    return;
  }

  std::ostringstream ss;
  ss << "OpenGL texture upload failed with error " << firstError << " (" << openGLErrorMessage(firstError) << ")";
  for (GLenum error = glGetError(); error != GL_NO_ERROR; error = glGetError()) {
    ss << ", followed by " << error << " (" << openGLErrorMessage(error) << ")";
  }
  ss << "; target=" << target << ", internalFormat=" << internalFormat << ", size=" << glm::to_string(size)
     << ", format=" << format << ", type=" << type;
  throwDebug(ss.str());
}

void throwIfTextureSizeExceedsLimits(Target target, const glm::ivec3& size)
{
  const TextureLimits limits = queryTextureLimits();
  bool valid = true;
  std::string limitName;
  GLint limitValue = 0;

  switch (target) {
    case Target::Texture1D:
      valid = size.x <= limits.maxTextureSize;
      limitName = "GL_MAX_TEXTURE_SIZE";
      limitValue = limits.maxTextureSize;
      break;
    case Target::TextureRectangle:
      valid = size.x <= limits.maxRectangleTextureSize && size.y <= limits.maxRectangleTextureSize;
      limitName = "GL_MAX_RECTANGLE_TEXTURE_SIZE";
      limitValue = limits.maxRectangleTextureSize;
      break;
    case Target::Texture2D:
    case Target::Texture2DMultisample:
      valid = size.x <= limits.maxTextureSize && size.y <= limits.maxTextureSize;
      limitName = "GL_MAX_TEXTURE_SIZE";
      limitValue = limits.maxTextureSize;
      break;
    case Target::Texture3D:
      valid =
        size.x <= limits.max3DTextureSize && size.y <= limits.max3DTextureSize && size.z <= limits.max3DTextureSize;
      limitName = "GL_MAX_3D_TEXTURE_SIZE";
      limitValue = limits.max3DTextureSize;
      break;
    case Target::Texture1DArray:
      valid = size.x <= limits.maxTextureSize && size.y <= limits.maxArrayTextureLayers;
      limitName = "GL_MAX_TEXTURE_SIZE / GL_MAX_ARRAY_TEXTURE_LAYERS";
      limitValue = std::min(limits.maxTextureSize, limits.maxArrayTextureLayers);
      break;
    case Target::Texture2DArray:
    case Target::Texture2DMultisampleArray:
      valid =
        size.x <= limits.maxTextureSize && size.y <= limits.maxTextureSize && size.z <= limits.maxArrayTextureLayers;
      limitName = "GL_MAX_TEXTURE_SIZE / GL_MAX_ARRAY_TEXTURE_LAYERS";
      limitValue = std::min(limits.maxTextureSize, limits.maxArrayTextureLayers);
      break;
    case Target::TextureCubeMap:
      valid = size.x <= limits.maxCubeMapTextureSize && size.y <= limits.maxCubeMapTextureSize;
      limitName = "GL_MAX_CUBE_MAP_TEXTURE_SIZE";
      limitValue = limits.maxCubeMapTextureSize;
      break;
    case Target::TextureBuffer:
      return;
  }

  if (!valid) {
    std::ostringstream ss;
    ss << "Texture size " << glm::to_string(size) << " exceeds " << limitName << " for target "
       << underlyingType_asInt32(target) << " (limit " << limitValue << ")";
    throwDebug(ss.str());
  }
}

} // namespace

class GLTexture::PixelStoreGuard
{
public:
  PixelStoreGuard(const bool pack, const std::optional<PixelStoreSettings>& requested) : m_pack(pack)
  {
    if (!requested) {
      return;
    }
    validatePixelStoreSettings(*requested);
    m_previous = pack ? getPixelPackSettings() : getPixelUnpackSettings();
    pack ? applyPixelPackSettings(*requested) : applyPixelUnpackSettings(*requested);
  }

  PixelStoreGuard(const PixelStoreGuard&) = delete;
  PixelStoreGuard& operator=(const PixelStoreGuard&) = delete;

  ~PixelStoreGuard()
  {
    if (m_previous) {
      m_pack ? applyPixelPackSettings(*m_previous) : applyPixelUnpackSettings(*m_previous);
    }
  }

private:
  bool m_pack;
  std::optional<PixelStoreSettings> m_previous;
};

const std::unordered_map<Target, Binding> GLTexture::s_bindingMap = {
  {Target::Texture1D, Binding::TextureBinding1D},
  {Target::Texture2D, Binding::TextureBinding2D},
  {Target::Texture3D, Binding::TextureBinding3D},
  {Target::TextureCubeMap, Binding::TextureBindingCubeMap},
  {Target::Texture1DArray, Binding::TextureBinding1DArray},
  {Target::Texture2DArray, Binding::TextureBinding2DArray},
  {Target::Texture2DMultisample, Binding::TextureBinding2DMultisample},
  {Target::TextureRectangle, Binding::TextureBindingRectangle},
  {Target::Texture2DMultisampleArray, Binding::TextureBinding2DMultisampleArray},
  {Target::TextureBuffer, Binding::TextureBindingBuffer}};

// Sized internal normalized formats:

const std::unordered_map<ComponentType, SizedInternalFormat>
  GLTexture::s_componentTypeToSizedInternalNormalizedRedFormatMap = {
    {ComponentType::Int8, SizedInternalFormat::R8_SNorm},
    {ComponentType::UInt8, SizedInternalFormat::R8_UNorm},
    {ComponentType::Int16, SizedInternalFormat::R16_SNorm},
    {ComponentType::UInt16, SizedInternalFormat::R16_UNorm},
    {ComponentType::Int32, SizedInternalFormat::R32F},
    {ComponentType::UInt32, SizedInternalFormat::R32F},
    {ComponentType::Float32, SizedInternalFormat::R32F}};

const std::unordered_map<ComponentType, tex::SizedInternalFormat>
  GLTexture::s_componentTypeToSizedInternalNormalizedRGFormatMap = {
    {ComponentType::Int8, SizedInternalFormat::RG8_SNorm},
    {ComponentType::UInt8, SizedInternalFormat::RG8_UNorm},
    {ComponentType::Int16, SizedInternalFormat::RG16_SNorm},
    {ComponentType::UInt16, SizedInternalFormat::RG16_UNorm},
    {ComponentType::Int32, SizedInternalFormat::RG32F},
    {ComponentType::UInt32, SizedInternalFormat::RG32F},
    {ComponentType::Float32, SizedInternalFormat::RG32F}};

const std::unordered_map<ComponentType, tex::SizedInternalFormat>
  GLTexture::s_componentTypeToSizedInternalNormalizedRGBFormatMap = {
    {ComponentType::Int8, SizedInternalFormat::RGB8_SNorm},
    {ComponentType::UInt8, SizedInternalFormat::RGB8_UNorm},
    {ComponentType::Int16, SizedInternalFormat::RGB16_SNorm},
    {ComponentType::UInt16, SizedInternalFormat::RGB16_UNorm},
    {ComponentType::Int32, SizedInternalFormat::RGB32F},
    {ComponentType::UInt32, SizedInternalFormat::RGB32F},
    {ComponentType::Float32, SizedInternalFormat::RGB32F}};

const std::unordered_map<ComponentType, tex::SizedInternalFormat>
  GLTexture::s_componentTypeToSizedInternalNormalizedRGBAFormatMap = {
    {ComponentType::Int8, SizedInternalFormat::RGBA8_SNorm},
    {ComponentType::UInt8, SizedInternalFormat::RGBA8_UNorm},
    {ComponentType::Int16, SizedInternalFormat::RGBA16_SNorm},
    {ComponentType::UInt16, SizedInternalFormat::RGBA16_UNorm},
    {ComponentType::Int32, SizedInternalFormat::RGBA32F},
    {ComponentType::UInt32, SizedInternalFormat::RGBA32F},
    {ComponentType::Float32, SizedInternalFormat::RGBA32F}};

// Sized internal non-normalized formats:

const std::unordered_map<ComponentType, SizedInternalFormat> GLTexture::s_componentTypeToSizedInternalRedFormatMap = {
  {ComponentType::Int8, SizedInternalFormat::R8I},
  {ComponentType::UInt8, SizedInternalFormat::R8U},
  {ComponentType::Int16, SizedInternalFormat::R16I},
  {ComponentType::UInt16, SizedInternalFormat::R16U},
  {ComponentType::Int32, SizedInternalFormat::R32I},
  {ComponentType::UInt32, SizedInternalFormat::R32U},
  {ComponentType::Float32, SizedInternalFormat::R32F}};

const std::unordered_map<ComponentType, SizedInternalFormat> GLTexture::s_componentTypeToSizedInternalRGFormatMap = {
  {ComponentType::Int8, SizedInternalFormat::RG8I},
  {ComponentType::UInt8, SizedInternalFormat::RG8U},
  {ComponentType::Int16, SizedInternalFormat::RG16I},
  {ComponentType::UInt16, SizedInternalFormat::RG16U},
  {ComponentType::Int32, SizedInternalFormat::RG32I},
  {ComponentType::UInt32, SizedInternalFormat::RG32U},
  {ComponentType::Float32, SizedInternalFormat::RG32F}};

const std::unordered_map<ComponentType, SizedInternalFormat> GLTexture::s_componentTypeToSizedInternalRGBFormatMap = {
  {ComponentType::Int8, SizedInternalFormat::RGB8I},
  {ComponentType::UInt8, SizedInternalFormat::RGB8U},
  {ComponentType::Int16, SizedInternalFormat::RGB16I},
  {ComponentType::UInt16, SizedInternalFormat::RGB16U},
  {ComponentType::Int32, SizedInternalFormat::RGB32I},
  {ComponentType::UInt32, SizedInternalFormat::RGB32U},
  {ComponentType::Float32, SizedInternalFormat::RGB32F}};

const std::unordered_map<ComponentType, SizedInternalFormat> GLTexture::s_componentTypeToSizedInternalRGBAFormatMap = {
  {ComponentType::Int8, SizedInternalFormat::RGBA8I},
  {ComponentType::UInt8, SizedInternalFormat::RGBA8U},
  {ComponentType::Int16, SizedInternalFormat::RGBA16I},
  {ComponentType::UInt16, SizedInternalFormat::RGBA16U},
  {ComponentType::Int32, SizedInternalFormat::RGBA32I},
  {ComponentType::UInt32, SizedInternalFormat::RGBA32U},
  {ComponentType::Float32, SizedInternalFormat::RGBA32F}};

// Normalized buffer pixel formats:

const std::unordered_map<ComponentType, BufferPixelFormat>
  GLTexture::s_componentTypeToBufferPixelRedNormalizedFormatMap = {
    {ComponentType::Int8, BufferPixelFormat::Red},
    {ComponentType::UInt8, BufferPixelFormat::Red},
    {ComponentType::Int16, BufferPixelFormat::Red},
    {ComponentType::UInt16, BufferPixelFormat::Red},
    {ComponentType::Int32, BufferPixelFormat::Red},
    {ComponentType::UInt32, BufferPixelFormat::Red},
    {ComponentType::Float32, BufferPixelFormat::Red}};

const std::unordered_map<ComponentType, BufferPixelFormat>
  GLTexture::s_componentTypeToBufferPixelRGNormalizedFormatMap = {
    {ComponentType::Int8, BufferPixelFormat::RG},
    {ComponentType::UInt8, BufferPixelFormat::RG},
    {ComponentType::Int16, BufferPixelFormat::RG},
    {ComponentType::UInt16, BufferPixelFormat::RG},
    {ComponentType::Int32, BufferPixelFormat::RG},
    {ComponentType::UInt32, BufferPixelFormat::RG},
    {ComponentType::Float32, BufferPixelFormat::RG}};

const std::unordered_map<ComponentType, BufferPixelFormat>
  GLTexture::s_componentTypeToBufferPixelRGBNormalizedFormatMap = {
    {ComponentType::Int8, BufferPixelFormat::RGB},
    {ComponentType::UInt8, BufferPixelFormat::RGB},
    {ComponentType::Int16, BufferPixelFormat::RGB},
    {ComponentType::UInt16, BufferPixelFormat::RGB},
    {ComponentType::Int32, BufferPixelFormat::RGB},
    {ComponentType::UInt32, BufferPixelFormat::RGB},
    {ComponentType::Float32, BufferPixelFormat::RGB}};

const std::unordered_map<ComponentType, BufferPixelFormat>
  GLTexture::s_componentTypeToBufferPixelRGBANormalizedFormatMap = {
    {ComponentType::Int8, BufferPixelFormat::RGBA},
    {ComponentType::UInt8, BufferPixelFormat::RGBA},
    {ComponentType::Int16, BufferPixelFormat::RGBA},
    {ComponentType::UInt16, BufferPixelFormat::RGBA},
    {ComponentType::Int32, BufferPixelFormat::RGBA},
    {ComponentType::UInt32, BufferPixelFormat::RGBA},
    {ComponentType::Float32, BufferPixelFormat::RGBA}};

// Non-normalized buffer pixel formats:

const std::unordered_map<ComponentType, BufferPixelFormat> GLTexture::s_componentTypeToBufferPixelRedFormatMap = {
  {ComponentType::Int8, BufferPixelFormat::Red_Integer},
  {ComponentType::UInt8, BufferPixelFormat::Red_Integer},
  {ComponentType::Int16, BufferPixelFormat::Red_Integer},
  {ComponentType::UInt16, BufferPixelFormat::Red_Integer},
  {ComponentType::Int32, BufferPixelFormat::Red_Integer},
  {ComponentType::UInt32, BufferPixelFormat::Red_Integer},
  {ComponentType::Float32, BufferPixelFormat::Red}};

const std::unordered_map<ComponentType, BufferPixelFormat> GLTexture::s_componentTypeToBufferPixelRGFormatMap = {
  {ComponentType::Int8, BufferPixelFormat::RG_Integer},
  {ComponentType::UInt8, BufferPixelFormat::RG_Integer},
  {ComponentType::Int16, BufferPixelFormat::RG_Integer},
  {ComponentType::UInt16, BufferPixelFormat::RG_Integer},
  {ComponentType::Int32, BufferPixelFormat::RG_Integer},
  {ComponentType::UInt32, BufferPixelFormat::RG_Integer},
  {ComponentType::Float32, BufferPixelFormat::RG}};

const std::unordered_map<ComponentType, BufferPixelFormat> GLTexture::s_componentTypeToBufferPixelRGBFormatMap = {
  {ComponentType::Int8, BufferPixelFormat::RGB_Integer},
  {ComponentType::UInt8, BufferPixelFormat::RGB_Integer},
  {ComponentType::Int16, BufferPixelFormat::RGB_Integer},
  {ComponentType::UInt16, BufferPixelFormat::RGB_Integer},
  {ComponentType::Int32, BufferPixelFormat::RGB_Integer},
  {ComponentType::UInt32, BufferPixelFormat::RGB_Integer},
  {ComponentType::Float32, BufferPixelFormat::RGB}};

const std::unordered_map<ComponentType, BufferPixelFormat> GLTexture::s_componentTypeToBufferPixelRGBAFormatMap = {
  {ComponentType::Int8, BufferPixelFormat::RGBA_Integer},
  {ComponentType::UInt8, BufferPixelFormat::RGBA_Integer},
  {ComponentType::Int16, BufferPixelFormat::RGBA_Integer},
  {ComponentType::UInt16, BufferPixelFormat::RGBA_Integer},
  {ComponentType::Int32, BufferPixelFormat::RGBA_Integer},
  {ComponentType::UInt32, BufferPixelFormat::RGBA_Integer},
  {ComponentType::Float32, BufferPixelFormat::RGBA}};

// Buffer pixel data type:

const std::unordered_map<ComponentType, BufferPixelDataType> GLTexture::s_componentTypeToBufferPixelDataTypeMap = {
  {ComponentType::Int8, BufferPixelDataType::Int8},
  {ComponentType::UInt8, BufferPixelDataType::UInt8},
  {ComponentType::Int16, BufferPixelDataType::Int16},
  {ComponentType::UInt16, BufferPixelDataType::UInt16},
  {ComponentType::Int32, BufferPixelDataType::Int32},
  {ComponentType::UInt32, BufferPixelDataType::UInt32},
  {ComponentType::Float32, BufferPixelDataType::Float32}};

SizedInternalFormat GLTexture::getSizedInternalNormalizedRedFormat(const ComponentType& componentType)
{
  return s_componentTypeToSizedInternalNormalizedRedFormatMap.at(componentType);
}

SizedInternalFormat GLTexture::getSizedInternalNormalizedRGFormat(const ComponentType& componentType)
{
  return s_componentTypeToSizedInternalNormalizedRGFormatMap.at(componentType);
}

SizedInternalFormat GLTexture::getSizedInternalNormalizedRGBFormat(const ComponentType& componentType)
{
  return s_componentTypeToSizedInternalNormalizedRGBFormatMap.at(componentType);
}

SizedInternalFormat GLTexture::getSizedInternalNormalizedRGBAFormat(const ComponentType& componentType)
{
  return s_componentTypeToSizedInternalNormalizedRGBAFormatMap.at(componentType);
}

SizedInternalFormat GLTexture::getSizedInternalRedFormat(const ComponentType& componentType)
{
  return s_componentTypeToSizedInternalRedFormatMap.at(componentType);
}

SizedInternalFormat GLTexture::getSizedInternalRGFormat(const ComponentType& componentType)
{
  return s_componentTypeToSizedInternalRGFormatMap.at(componentType);
}

SizedInternalFormat GLTexture::getSizedInternalRGBFormat(const ComponentType& componentType)
{
  return s_componentTypeToSizedInternalRGBFormatMap.at(componentType);
}

SizedInternalFormat GLTexture::getSizedInternalRGBAFormat(const ComponentType& componentType)
{
  return s_componentTypeToSizedInternalRGBAFormatMap.at(componentType);
}

BufferPixelFormat GLTexture::getBufferPixelNormalizedRedFormat(const ComponentType& componentType)
{
  return s_componentTypeToBufferPixelRedNormalizedFormatMap.at(componentType);
}

BufferPixelFormat GLTexture::getBufferPixelNormalizedRGFormat(const ComponentType& componentType)
{
  return s_componentTypeToBufferPixelRGNormalizedFormatMap.at(componentType);
}

BufferPixelFormat GLTexture::getBufferPixelNormalizedRGBFormat(const ComponentType& componentType)
{
  return s_componentTypeToBufferPixelRGBNormalizedFormatMap.at(componentType);
}

BufferPixelFormat GLTexture::getBufferPixelNormalizedRGBAFormat(const ComponentType& componentType)
{
  return s_componentTypeToBufferPixelRGBANormalizedFormatMap.at(componentType);
}

BufferPixelFormat GLTexture::getBufferPixelRedFormat(const ComponentType& componentType)
{
  return s_componentTypeToBufferPixelRedFormatMap.at(componentType);
}

BufferPixelFormat GLTexture::getBufferPixelRGFormat(const ComponentType& componentType)
{
  return s_componentTypeToBufferPixelRGFormatMap.at(componentType);
}

BufferPixelFormat GLTexture::getBufferPixelRGBFormat(const ComponentType& componentType)
{
  return s_componentTypeToBufferPixelRGBFormatMap.at(componentType);
}

BufferPixelFormat GLTexture::getBufferPixelRGBAFormat(const ComponentType& componentType)
{
  return s_componentTypeToBufferPixelRGBAFormatMap.at(componentType);
}

BufferPixelDataType GLTexture::getBufferPixelDataType(const ComponentType& componentType)
{
  return s_componentTypeToBufferPixelDataTypeMap.at(componentType);
}

GLTexture::Binder::Binder(GLTexture& texture) : m_texture(texture), m_boundID(0)
{
  if (m_texture.m_id == 0u) {
    throwDebug("Cannot bind a texture before generating it");
  }
  glGetIntegerv(underlyingType(s_bindingMap.at(m_texture.m_target)), &m_boundID);
  glBindTexture(m_texture.m_targetEnum, static_cast<GLuint>(m_texture.m_id));
}

GLTexture::Binder::~Binder()
{
  glBindTexture(m_texture.m_targetEnum, static_cast<GLuint>(m_boundID));
}

GLTexture::GLTexture(
  Target target,
  MultisampleSettings multisampleSettings,
  std::optional<PixelStoreSettings> pixelPackSettings,
  std::optional<PixelStoreSettings> pixelUnpackSettings)
  : m_target(target)
  , m_targetEnum(underlyingType(m_target))
  , m_id(0)
  , m_size(1)
  , m_multisampleSettings(multisampleSettings)
  , m_pixelPackSettings(pixelPackSettings)
  , m_pixelUnpackSettings(pixelUnpackSettings)
{
}

GLTexture::GLTexture(GLTexture&& other) noexcept
  : m_target(other.m_target)
  , m_targetEnum(other.m_targetEnum)
  , m_id(other.m_id)
  , m_size(other.m_size)
  , m_hasAllocatedStorage(other.m_hasAllocatedStorage)
  , m_allocatedLevels(std::move(other.m_allocatedLevels))
  , m_allocatedCubeFaces(std::move(other.m_allocatedCubeFaces))
  , m_autoGenerateMipmaps(other.m_autoGenerateMipmaps)
  , m_loggedSuspiciousBind(other.m_loggedSuspiciousBind)
  , m_loggedUnitZeroBind(other.m_loggedUnitZeroBind)
  , m_lastInternalFormat(other.m_lastInternalFormat)
  , m_lastBufferFormat(other.m_lastBufferFormat)
  , m_lastBufferType(other.m_lastBufferType)
  , m_multisampleSettings(other.m_multisampleSettings)
  , m_pixelPackSettings(other.m_pixelPackSettings)
  , m_pixelUnpackSettings(other.m_pixelUnpackSettings)
{
  other.m_id = 0;
  other.m_size = glm::uvec3{1};
  other.m_hasAllocatedStorage = false;
  other.m_allocatedLevels.clear();
  other.m_allocatedCubeFaces.clear();
  other.m_autoGenerateMipmaps = false;
  other.m_loggedSuspiciousBind = false;
  other.m_loggedUnitZeroBind = false;
  other.m_lastInternalFormat = 0;
  other.m_lastBufferFormat = 0;
  other.m_lastBufferType = 0;
  other.m_multisampleSettings = MultisampleSettings();
  other.m_pixelPackSettings = std::nullopt;
  other.m_pixelUnpackSettings = std::nullopt;
}

GLTexture& GLTexture::operator=(GLTexture&& other) noexcept
{
  if (this != &other) {
    destroy();

    std::swap(m_target, other.m_target);
    std::swap(m_targetEnum, other.m_targetEnum);
    std::swap(m_id, other.m_id);
    std::swap(m_size, other.m_size);
    std::swap(m_hasAllocatedStorage, other.m_hasAllocatedStorage);
    std::swap(m_allocatedLevels, other.m_allocatedLevels);
    std::swap(m_allocatedCubeFaces, other.m_allocatedCubeFaces);
    std::swap(m_autoGenerateMipmaps, other.m_autoGenerateMipmaps);
    std::swap(m_loggedSuspiciousBind, other.m_loggedSuspiciousBind);
    std::swap(m_loggedUnitZeroBind, other.m_loggedUnitZeroBind);
    std::swap(m_lastInternalFormat, other.m_lastInternalFormat);
    std::swap(m_lastBufferFormat, other.m_lastBufferFormat);
    std::swap(m_lastBufferType, other.m_lastBufferType);
    std::swap(m_multisampleSettings, other.m_multisampleSettings);
    std::swap(m_pixelPackSettings, other.m_pixelPackSettings);
    std::swap(m_pixelUnpackSettings, other.m_pixelUnpackSettings);
  }

  return *this;
}

GLTexture::~GLTexture()
{
  destroy();
}

void GLTexture::generate()
{
  if (m_id != 0u) {
    return;
  }
  glGenTextures(1, &m_id);
  CHECK_GL_ERROR(m_errorChecker);
  if (supportsSamplingParameters(m_target)) {
    const Binder binder(*this);
    // OpenGL's default minification filter requires mipmaps. Entropy textures begin with base-level-only storage, so
    // choose a complete and predictable default until a caller explicitly requests another filter.
    glTexParameteri(m_targetEnum, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    CHECK_GL_ERROR(m_errorChecker);
  }
  labelTextureObject(m_id, m_target, m_size);
}

void GLTexture::destroy()
{
  if (m_id != 0u) {
    glDeleteTextures(1, &m_id);
  }
  m_id = 0;
  m_size = glm::uvec3{1};
  m_hasAllocatedStorage = false;
  m_allocatedLevels.clear();
  m_allocatedCubeFaces.clear();
  m_autoGenerateMipmaps = false;
  m_loggedSuspiciousBind = false;
  m_loggedUnitZeroBind = false;
  m_lastInternalFormat = 0;
  m_lastBufferFormat = 0;
  m_lastBufferType = 0;
  m_multisampleSettings = MultisampleSettings();
  m_pixelPackSettings = std::nullopt;
  m_pixelUnpackSettings = std::nullopt;
}

void GLTexture::bind(std::optional<uint32_t> textureUnit) const
{
  if (m_id == 0u) {
    throwDebug("Cannot bind a texture before generating it");
  }
  GLint previousTextureUnit = GL_TEXTURE0;
  if (textureUnit) {
    glGetIntegerv(GL_ACTIVE_TEXTURE, &previousTextureUnit);
    glActiveTexture(static_cast<GLenum>(GL_TEXTURE0 + *textureUnit));
  }

  glBindTexture(m_targetEnum, m_id);

  if (textureUnit) {
    glActiveTexture(static_cast<GLenum>(previousTextureUnit));
  }
  CHECK_GL_ERROR(m_errorChecker);

  if (textureUnit && *textureUnit == 0 && !m_loggedUnitZeroBind) {
    spdlog::trace(
      "Binding GL texture to unit 0: id={}, target={} ({}), size={}, allocated={}, internalFormat={}, format={}, "
      "type={}, autoMipmaps={}",
      m_id,
      m_targetEnum,
      textureTargetName(m_target),
      glm::to_string(m_size),
      m_hasAllocatedStorage,
      m_lastInternalFormat,
      m_lastBufferFormat,
      m_lastBufferType,
      m_autoGenerateMipmaps);
    m_loggedUnitZeroBind = true;
  }

  if (m_target != Target::TextureBuffer && !m_hasAllocatedStorage && !m_loggedSuspiciousBind) {
    spdlog::warn(
      "Binding suspicious GL texture: id={}, target={} ({}), unit={}, size={}, allocated={}, mipmaps={}",
      m_id,
      m_targetEnum,
      textureTargetName(m_target),
      textureUnit ? static_cast<int>(*textureUnit) : -1,
      glm::to_string(m_size),
      m_hasAllocatedStorage,
      m_autoGenerateMipmaps);
    m_loggedSuspiciousBind = true;
  }
}

bool GLTexture::isBound(std::optional<uint32_t> textureUnit) const
{
  if (m_id == 0u) {
    return false;
  }
  GLint prevUnit = 0;

  if (textureUnit) {
    glGetIntegerv(GL_ACTIVE_TEXTURE, &prevUnit);
    glActiveTexture(GL_TEXTURE0 + *textureUnit);
  }

  GLint boundID = 0;
  glGetIntegerv(underlyingType(s_bindingMap.at(m_target)), &boundID);

  const bool result = (static_cast<GLuint>(boundID) == m_id);

  if (textureUnit) {
    glActiveTexture(static_cast<GLenum>(prevUnit));
  }

  CHECK_GL_ERROR(m_errorChecker);

  return result;
}

void GLTexture::unbind(std::optional<uint32_t> textureUnit) const
{
  if (0 == m_id) {
    return;
  }

  if (textureUnit) {
    GLint previousTextureUnit = GL_TEXTURE0;
    glGetIntegerv(GL_ACTIVE_TEXTURE, &previousTextureUnit);
    glActiveTexture(static_cast<GLenum>(GL_TEXTURE0 + *textureUnit));
    GLint boundId = 0;
    glGetIntegerv(underlyingType(s_bindingMap.at(m_target)), &boundId);
    if (static_cast<GLuint>(boundId) == m_id) {
      glBindTexture(m_targetEnum, 0);
    }
    glActiveTexture(static_cast<GLenum>(previousTextureUnit));
    CHECK_GL_ERROR(m_errorChecker);
    return;
  }

  GLint boundId = 0;
  glGetIntegerv(underlyingType(s_bindingMap.at(m_target)), &boundId);
  if (static_cast<GLuint>(boundId) == m_id) {
    glBindTexture(m_targetEnum, 0);
  }
  CHECK_GL_ERROR(m_errorChecker);
}

Target GLTexture::target() const
{
  return m_target;
}

GLuint GLTexture::id() const
{
  return m_id;
}

glm::uvec3 GLTexture::size() const
{
  return m_size;
}

void GLTexture::setSize(const glm::uvec3& sizeArg)
{
  if (glm::any(glm::lessThan(sizeArg, glm::uvec3{1}))) {
    std::ostringstream ss;
    ss << "Invalid texture size " << glm::to_string(sizeArg) << std::ends;
    throwDebug(ss.str());
  }
  if (glm::any(glm::greaterThan(sizeArg, glm::uvec3{static_cast<uint32_t>(std::numeric_limits<GLsizei>::max())}))) {
    throwDebug("Texture dimensions exceed GLsizei range");
  }
  validateTextureDimensions(m_target, sizeArg);

  if (m_size != sizeArg) {
    m_hasAllocatedStorage = false;
    m_allocatedLevels.clear();
    m_allocatedCubeFaces.clear();
    m_loggedSuspiciousBind = false;
    m_loggedUnitZeroBind = false;
    m_size = sizeArg;
    labelTextureObject(m_id, m_target, m_size);
  }
}

void GLTexture::setData(
  GLint level,
  const SizedInternalFormat& internalFormat,
  const BufferPixelFormat& format,
  const BufferPixelDataType& type,
  const GLvoid* data)
{
  if (m_id == 0u) {
    throwDebug("Cannot allocate texture storage before generating the texture");
  }
  if (Target::TextureCubeMap == m_target || Target::TextureBuffer == m_target) {
    throwDebug("Invalid texture target type ");
  }
  if (level != 0) {
    throwDebug("Texture storage must be allocated at base mipmap level zero");
  }

  const GLint _internalFormat = underlyingType_asInt32(internalFormat);
  const GLenum _format = underlyingType(format);
  const GLenum _type = underlyingType(type);
  const glm::ivec3 _size = textureSizeForLevel(m_target, m_size, level);

  if (
    (Target::Texture2DMultisample == m_target || Target::Texture2DMultisampleArray == m_target) &&
    m_multisampleSettings.m_numSamples <= 0)
  {
    throwDebug("Multisample textures require at least one sample");
  }
  if (Target::Texture2DMultisample == m_target || Target::Texture2DMultisampleArray == m_target) {
    if (data != nullptr) {
      throwDebug("Multisample texture allocation does not accept initial pixel data");
    }
    GLint maxSamples = 0;
    glGetIntegerv(GL_MAX_SAMPLES, &maxSamples);
    if (m_multisampleSettings.m_numSamples > maxSamples) {
      throwDebug("Requested multisample texture sample count exceeds GL_MAX_SAMPLES");
    }
  }

  throwIfTextureSizeExceedsLimits(m_target, _size);
  // Do not silently discard an error queued by an earlier raw OpenGL call. Checking here keeps a stale error from
  // being misreported as a texture-upload failure while preserving its original failure signal.
  CHECK_GL_ERROR(m_errorChecker);

  Binder binder(*this);

  const PixelStoreGuard pixelStoreGuard(false, m_pixelUnpackSettings);

  switch (m_target) {
    case Target::Texture1D: {
      glTexImage1D(m_targetEnum, level, _internalFormat, _size.x, 0, _format, _type, data);
      break;
    }
    case Target::Texture2D: {
      glTexImage2D(m_targetEnum, level, _internalFormat, _size.x, _size.y, 0, _format, _type, data);
      break;
    }
    case Target::Texture3D: {
      glTexImage3D(m_targetEnum, level, _internalFormat, _size.x, _size.y, _size.z, 0, _format, _type, data);
      break;
    }
    case Target::Texture1DArray: {
      glTexImage2D(m_targetEnum, level, _internalFormat, _size.x, _size.y, 0, _format, _type, data);
      break;
    }
    case Target::Texture2DArray: {
      glTexImage3D(m_targetEnum, level, _internalFormat, _size.x, _size.y, _size.z, 0, _format, _type, data);
      break;
    }
    case Target::Texture2DMultisample: {
      glTexImage2DMultisample(
        m_targetEnum,
        m_multisampleSettings.m_numSamples,
        _internalFormat,
        _size.x,
        _size.y,
        m_multisampleSettings.m_fixedSampleLocations);
      break;
    }
    case Target::TextureRectangle: {
      glTexImage2D(m_targetEnum, 0, _internalFormat, _size.x, _size.y, 0, _format, _type, data);
      break;
    }
    case Target::Texture2DMultisampleArray: {
      glTexImage3DMultisample(
        m_targetEnum,
        m_multisampleSettings.m_numSamples,
        _internalFormat,
        _size.x,
        _size.y,
        _size.z,
        m_multisampleSettings.m_fixedSampleLocations);
      break;
    }
    case Target::TextureCubeMap:
    case Target::TextureBuffer: {
      break;
    }
  }

  throwIfOpenGLErrorAfterTextureUpload(m_targetEnum, _internalFormat, _size, _format, _type);
  m_hasAllocatedStorage = true;
  m_allocatedLevels.insert(level);
  m_loggedSuspiciousBind = false;
  m_loggedUnitZeroBind = false;
  m_lastInternalFormat = _internalFormat;
  m_lastBufferFormat = _format;
  m_lastBufferType = _type;
  labelTextureObject(m_id, m_target, m_size);

  if (textureTargetSupportsMipmaps(m_target)) {
    // Render-target and image fallback textures usually allocate only level 0. Advertising extra mip levels makes the
    // texture incomplete on some drivers, which can later surface as framebuffer or sampler errors.
    setTextureLevelRange(
      m_targetEnum,
      m_autoGenerateMipmaps ? mipmapMaxLevelForTarget(m_target, glm::ivec3{m_size}) : level);

    if (m_autoGenerateMipmaps) {
      glGenerateMipmap(m_targetEnum);
    }
  }

  spdlog::debug(
    "Uploaded GL texture: id={}, target={} ({}), level={}, size={}, internalFormat={}, format={}, type={}, "
    "mipmapMaxLevel={}, autoMipmaps={}",
    m_id,
    m_targetEnum,
    textureTargetName(m_target),
    level,
    glm::to_string(m_size),
    _internalFormat,
    _format,
    _type,
    textureTargetSupportsMipmaps(m_target)
      ? (m_autoGenerateMipmaps ? mipmapMaxLevelForTarget(m_target, glm::ivec3{m_size}) : level)
      : 0,
    m_autoGenerateMipmaps);

  m_errorChecker(__FILE__, __FUNCTION__, __LINE__);
}

void GLTexture::setSubData(
  GLint level,
  const glm::uvec3& offset,
  const glm::uvec3& sizeArg,
  const BufferPixelFormat& format,
  const BufferPixelDataType& type,
  const GLvoid* data)
{
  if (
    Target::Texture2DMultisample == m_target || Target::Texture2DMultisampleArray == m_target ||
    Target::TextureCubeMap == m_target || Target::TextureBuffer == m_target)
  {
    throwDebug("Invalid texture target type ");
  }
  if (level < 0) {
    throwDebug("Texture sub-data mipmap level cannot be negative");
  }
  if (level != 0) {
    throwDebug("Texture sub-data updates must target base mipmap level zero");
  }
  if (!data) {
    throwDebug("Texture sub-data pointer cannot be null");
  }
  if (!m_hasAllocatedStorage) {
    throwDebug("Cannot write texture sub-data before allocating texture storage");
  }
  const bool levelAllocated =
    m_allocatedLevels.contains(level) || (m_autoGenerateMipmaps && m_allocatedLevels.contains(0) &&
                                          level <= mipmapMaxLevelForTarget(m_target, glm::ivec3{m_size}));
  if (!levelAllocated) {
    throwDebug("Cannot write texture sub-data to an unallocated mipmap level");
  }
  if (glm::any(glm::equal(sizeArg, glm::uvec3{0u}))) {
    throwDebug("Texture sub-data dimensions must all be greater than zero");
  }
  const glm::ivec3 levelSizeSigned = textureSizeForLevel(m_target, m_size, level);
  const glm::uvec3 levelSize{levelSizeSigned};
  for (int axis = 0; axis < 3; ++axis) {
    if (offset[axis] > levelSize[axis] || sizeArg[axis] > levelSize[axis] - offset[axis]) {
      throwDebug("Texture sub-data region exceeds allocated texture bounds");
    }
  }
  if (Target::Texture1D == m_target && (offset.y != 0u || offset.z != 0u || sizeArg.y != 1u || sizeArg.z != 1u)) {
    throwDebug("One-dimensional texture sub-data must use singleton y and z extents");
  }
  if (
    (Target::Texture2D == m_target || Target::TextureRectangle == m_target || Target::Texture1DArray == m_target) &&
    (offset.z != 0u || sizeArg.z != 1u))
  {
    throwDebug("Two-dimensional texture sub-data must use a singleton z extent");
  }

  const GLenum _format = underlyingType(format);
  const GLenum _type = underlyingType(type);
  const glm::ivec3 _offset(offset);
  const glm::ivec3 _size(sizeArg);

  Binder binder(*this);

  const PixelStoreGuard pixelStoreGuard(false, m_pixelUnpackSettings);

  switch (m_target) {
    case Target::Texture1D:
      glTexSubImage1D(m_targetEnum, level, _offset.x, _size.x, _format, _type, data);
      break;

    case Target::Texture2D:
    case Target::TextureRectangle:
      glTexSubImage2D(m_targetEnum, level, _offset.x, _offset.y, _size.x, _size.y, _format, _type, data);
      break;

    case Target::Texture3D:
      glTexSubImage3D(
        m_targetEnum,
        level,
        _offset.x,
        _offset.y,
        _offset.z,
        _size.x,
        _size.y,
        _size.z,
        _format,
        _type,
        data);
      break;

    case Target::Texture1DArray:
      glTexSubImage2D(m_targetEnum, level, _offset.x, _offset.y, _size.x, _size.y, _format, _type, data);
      break;

    case Target::Texture2DArray:
      glTexSubImage3D(
        m_targetEnum,
        level,
        _offset.x,
        _offset.y,
        _offset.z,
        _size.x,
        _size.y,
        _size.z,
        _format,
        _type,
        data);
      break;

    case Target::Texture2DMultisample:
    case Target::Texture2DMultisampleArray:
    case Target::TextureCubeMap:
    case Target::TextureBuffer:
      break;
  }

  if (m_autoGenerateMipmaps) {
    glGenerateMipmap(m_targetEnum);
  }

  m_errorChecker(__FILE__, __FUNCTION__, __LINE__);
}

void GLTexture::setCubeMapFaceData(
  const CubeMapFace& face,
  GLint level,
  const SizedInternalFormat& internalFormat,
  const BufferPixelFormat& format,
  const BufferPixelDataType& type,
  const GLvoid* data)
{
  if (m_target != Target::TextureCubeMap) {
    throwDebug("Cube-map face data requires a cube-map texture");
  }
  if (m_id == 0u) {
    throwDebug("Cannot allocate cube-map storage before generating the texture");
  }
  if (level != 0) {
    throwDebug("Cube-map storage must be allocated at base mipmap level zero");
  }

  const glm::ivec3 _size = textureSizeForLevel(m_target, m_size, level);
  if (_size.x != _size.y) {
    throwDebug("Cube-map faces must be square");
  }
  throwIfTextureSizeExceedsLimits(m_target, _size);

  Binder binder(*this);
  const PixelStoreGuard pixelStoreGuard(false, m_pixelUnpackSettings);

  const GLint newInternalFormat = underlyingType_asInt32(internalFormat);
  const GLenum newBufferFormat = underlyingType(format);
  const GLenum newBufferType = underlyingType(type);
  if (m_hasAllocatedStorage && m_lastInternalFormat != newInternalFormat) {
    throwDebug("All faces of a cube-map texture must use the same internal format");
  }

  glTexImage2D(
    underlyingType(face),
    level,
    newInternalFormat,
    _size.x,
    _size.y,
    0,
    newBufferFormat,
    newBufferType,
    data);
  CHECK_GL_ERROR(m_errorChecker);
  m_hasAllocatedStorage = true;
  m_allocatedCubeFaces[level] |= cubeMapFaceBit(face);
  m_lastInternalFormat = newInternalFormat;
  m_lastBufferFormat = newBufferFormat;
  m_lastBufferType = newBufferType;
  labelTextureObject(m_id, m_target, m_size);

  setTextureLevelRange(
    m_targetEnum,
    m_autoGenerateMipmaps ? mipmapMaxLevelForTarget(m_target, glm::ivec3{m_size}) : level);

  constexpr uint8_t allCubeFaces = (1u << 6u) - 1u;
  if (m_autoGenerateMipmaps && level == 0 && m_allocatedCubeFaces.at(level) == allCubeFaces) {
    glGenerateMipmap(m_targetEnum);
  }

  m_errorChecker(__FILE__, __FUNCTION__, __LINE__);
}

void GLTexture::readData(GLint level, const BufferPixelFormat& format, const BufferPixelDataType& type, GLvoid* data)
{
  if (
    Target::Texture2DMultisample == m_target || Target::Texture2DMultisampleArray == m_target ||
    Target::TextureCubeMap == m_target || Target::TextureBuffer == m_target)
  {
    throwDebug("Texture target does not support direct pixel reads");
  }
  if (!m_hasAllocatedStorage || data == nullptr) {
    throwDebug("Cannot read texture data without allocated storage and a destination buffer");
  }
  static_cast<void>(textureSizeForLevel(m_target, m_size, level));
  const bool levelAllocated =
    m_allocatedLevels.contains(level) || (m_autoGenerateMipmaps && m_allocatedLevels.contains(0) &&
                                          level <= mipmapMaxLevelForTarget(m_target, glm::ivec3{m_size}));
  if (!levelAllocated) {
    throwDebug("Cannot read an unallocated texture mipmap level");
  }

  Binder binder(*this);
  const PixelStoreGuard pixelStoreGuard(true, m_pixelPackSettings);

  glGetTexImage(m_targetEnum, level, underlyingType(format), underlyingType(type), data);
  CHECK_GL_ERROR(m_errorChecker);
}

void GLTexture::readCubeMapFaceData(
  const CubeMapFace& face,
  GLint level,
  const BufferPixelFormat& format,
  const BufferPixelDataType& type,
  GLvoid* data)
{
  if (m_target != Target::TextureCubeMap) {
    throwDebug("Cube-map face reads require a cube-map texture");
  }
  if (!m_hasAllocatedStorage || data == nullptr) {
    throwDebug("Cannot read cube-map data without allocated storage and a destination buffer");
  }
  static_cast<void>(textureSizeForLevel(m_target, m_size, level));
  constexpr uint8_t allCubeFaces = (1u << 6u) - 1u;
  const auto allocatedFaces = m_allocatedCubeFaces.find(level);
  const bool explicitlyAllocated =
    allocatedFaces != m_allocatedCubeFaces.end() && (allocatedFaces->second & cubeMapFaceBit(face)) != 0u;
  const bool generatedFromCompleteBase = level > 0 && m_autoGenerateMipmaps && m_allocatedCubeFaces.contains(0) &&
                                         m_allocatedCubeFaces.at(0) == allCubeFaces;
  if (!explicitlyAllocated && !generatedFromCompleteBase) {
    throwDebug("Cannot read an unallocated cube-map face");
  }

  Binder binder(*this);
  const PixelStoreGuard pixelStoreGuard(true, m_pixelPackSettings);

  glGetTexImage(underlyingType(face), level, underlyingType(format), underlyingType(type), data);

  CHECK_GL_ERROR(m_errorChecker);
}

void GLTexture::setMinificationFilter(const MinificationFilter& filter)
{
  if (!supportsSamplingParameters(m_target)) {
    throwDebug("Texture target does not support minification filtering");
  }
  if (Target::TextureRectangle == m_target && isMipmapFilter(filter)) {
    throwDebug("Rectangle textures do not support mipmap minification filters");
  }

  Binder binder(*this);

  glTexParameteri(m_targetEnum, GL_TEXTURE_MIN_FILTER, underlyingType_asInt32(filter));
  CHECK_GL_ERROR(m_errorChecker);
}

void GLTexture::setMagnificationFilter(const MagnificationFilter& filter)
{
  if (!supportsSamplingParameters(m_target)) {
    throwDebug("Texture target does not support magnification filtering");
  }

  Binder binder(*this);

  glTexParameteri(m_targetEnum, GL_TEXTURE_MAG_FILTER, underlyingType_asInt32(filter));
  CHECK_GL_ERROR(m_errorChecker);
}

void GLTexture::setSwizzleMask(
  const SwizzleValue& rValue,
  const SwizzleValue& gValue,
  const SwizzleValue& bValue,
  const SwizzleValue& aValue)
{
  if (!supportsSamplingParameters(m_target)) {
    throwDebug("Texture target does not support component swizzling");
  }
  const GLint mask[] = {
    static_cast<GLint>(underlyingType(rValue)),
    static_cast<GLint>(underlyingType(gValue)),
    static_cast<GLint>(underlyingType(bValue)),
    static_cast<GLint>(underlyingType(aValue))};

  Binder binder(*this);
  glTexParameteriv(m_targetEnum, GL_TEXTURE_SWIZZLE_RGBA, mask);
  CHECK_GL_ERROR(m_errorChecker);
}

void GLTexture::setWrapMode(const WrapMode& mode)
{
  if (!supportsSamplingParameters(m_target)) {
    throwDebug("Texture target does not support wrapping modes");
  }
  Binder binder(*this);

  if (Target::Texture1D == m_target || Target::Texture1DArray == m_target) {
    glTexParameteri(m_targetEnum, GL_TEXTURE_WRAP_S, underlyingType_asInt32(mode));
  }
  else if (
    Target::Texture2D == m_target || Target::Texture2DArray == m_target || Target::TextureRectangle == m_target ||
    Target::TextureCubeMap == m_target)
  {
    glTexParameteri(m_targetEnum, GL_TEXTURE_WRAP_S, underlyingType_asInt32(mode));
    glTexParameteri(m_targetEnum, GL_TEXTURE_WRAP_T, underlyingType_asInt32(mode));
  }
  else if (Target::Texture3D == m_target) {
    glTexParameteri(m_targetEnum, GL_TEXTURE_WRAP_S, underlyingType_asInt32(mode));
    glTexParameteri(m_targetEnum, GL_TEXTURE_WRAP_T, underlyingType_asInt32(mode));
    glTexParameteri(m_targetEnum, GL_TEXTURE_WRAP_R, underlyingType_asInt32(mode));
  }
  CHECK_GL_ERROR(m_errorChecker);
}

void GLTexture::setBorderColor(const glm::vec4& color)
{
  if (!supportsSamplingParameters(m_target)) {
    throwDebug("Texture target does not support border colors");
  }
  Binder binder(*this);
  glTexParameterfv(m_targetEnum, GL_TEXTURE_BORDER_COLOR, glm::value_ptr(color));
  CHECK_GL_ERROR(m_errorChecker);
}

void GLTexture::setAutoGenerateMipmaps(bool enabled)
{
  if (enabled && !textureTargetSupportsMipmaps(m_target)) {
    throwDebug("This texture target does not support mipmap generation");
  }
  m_autoGenerateMipmaps = enabled;

  if (textureTargetSupportsMipmaps(m_target)) {
    Binder binder(*this);
    setTextureLevelRange(
      m_targetEnum,
      m_autoGenerateMipmaps ? mipmapMaxLevelForTarget(m_target, glm::ivec3{m_size}) : 0);

    constexpr uint8_t allCubeFaces = (1u << 6u) - 1u;
    const bool cubeMapComplete = m_target != Target::TextureCubeMap ||
                                 (m_allocatedCubeFaces.contains(0) && m_allocatedCubeFaces.at(0) == allCubeFaces);
    if (m_autoGenerateMipmaps && m_hasAllocatedStorage && cubeMapComplete) {
      glGenerateMipmap(m_targetEnum);
    }
    CHECK_GL_ERROR(m_errorChecker);
  }
}

void GLTexture::setBufferTextureStorage(const GLint internalFormat, const GLuint bufferId, const std::size_t texelCount)
{
  if (m_target != Target::TextureBuffer) {
    throwDebug("Only texture-buffer objects can attach buffer storage");
  }

  const Binder binder(*this);
  glTexBuffer(m_targetEnum, static_cast<GLenum>(internalFormat), bufferId);
  CHECK_GL_ERROR(m_errorChecker);

  m_size =
    glm::uvec3{static_cast<uint32_t>(std::min<std::size_t>(texelCount, std::numeric_limits<uint32_t>::max())), 1u, 1u};
  m_hasAllocatedStorage = texelCount > 0;
  m_allocatedLevels.clear();
  m_allocatedCubeFaces.clear();
  m_loggedSuspiciousBind = false;
  m_loggedUnitZeroBind = false;
  m_lastInternalFormat = internalFormat;
  m_lastBufferFormat = 0;
  m_lastBufferType = 0;
  labelTextureObject(m_id, m_target, m_size);

  spdlog::debug(
    "Attached GL texture buffer storage: id={}, target={} ({}), texels={}, internalFormat={}",
    m_id,
    m_targetEnum,
    textureTargetName(m_target),
    texelCount,
    internalFormat);
}

void GLTexture::setPixelPackSettings(const PixelStoreSettings& settings)
{
  validatePixelStoreSettings(settings);
  m_pixelPackSettings = settings;
}

void GLTexture::setPixelUnpackSettings(const PixelStoreSettings& settings)
{
  validatePixelStoreSettings(settings);
  m_pixelUnpackSettings = settings;
}

GLTexture::PixelStoreSettings GLTexture::getPixelPackSettings()
{
  PixelStoreSettings settings;

  glGetIntegerv(GL_PACK_ALIGNMENT, &settings.m_alignment);
  glGetIntegerv(GL_PACK_SKIP_IMAGES, &settings.m_skipImages);
  glGetIntegerv(GL_PACK_SKIP_ROWS, &settings.m_skipRows);
  glGetIntegerv(GL_PACK_SKIP_PIXELS, &settings.m_skipPixels);
  glGetIntegerv(GL_PACK_IMAGE_HEIGHT, &settings.m_imageHeight);
  glGetIntegerv(GL_PACK_ROW_LENGTH, &settings.m_rowLength);
  glGetBooleanv(GL_PACK_LSB_FIRST, &settings.m_lsbFirst);
  glGetBooleanv(GL_PACK_SWAP_BYTES, &settings.m_swapBytes);

  return settings;
}

GLTexture::PixelStoreSettings GLTexture::getPixelUnpackSettings()
{
  PixelStoreSettings settings;

  glGetIntegerv(GL_UNPACK_ALIGNMENT, &settings.m_alignment);
  glGetIntegerv(GL_UNPACK_SKIP_IMAGES, &settings.m_skipImages);
  glGetIntegerv(GL_UNPACK_SKIP_ROWS, &settings.m_skipRows);
  glGetIntegerv(GL_UNPACK_SKIP_PIXELS, &settings.m_skipPixels);
  glGetIntegerv(GL_UNPACK_IMAGE_HEIGHT, &settings.m_imageHeight);
  glGetIntegerv(GL_UNPACK_ROW_LENGTH, &settings.m_rowLength);
  glGetBooleanv(GL_UNPACK_LSB_FIRST, &settings.m_lsbFirst);
  glGetBooleanv(GL_UNPACK_SWAP_BYTES, &settings.m_swapBytes);

  return settings;
}

void GLTexture::applyPixelPackSettings(const PixelStoreSettings& settings)
{
  glPixelStorei(GL_PACK_ALIGNMENT, settings.m_alignment);
  glPixelStorei(GL_PACK_SKIP_IMAGES, settings.m_skipImages);
  glPixelStorei(GL_PACK_SKIP_ROWS, settings.m_skipRows);
  glPixelStorei(GL_PACK_SKIP_PIXELS, settings.m_skipPixels);
  glPixelStorei(GL_PACK_IMAGE_HEIGHT, settings.m_imageHeight);
  glPixelStorei(GL_PACK_ROW_LENGTH, settings.m_rowLength);
  glPixelStorei(GL_PACK_LSB_FIRST, settings.m_lsbFirst);
  glPixelStorei(GL_PACK_SWAP_BYTES, settings.m_swapBytes);
}

void GLTexture::applyPixelUnpackSettings(const PixelStoreSettings& settings)
{
  glPixelStorei(GL_UNPACK_ALIGNMENT, settings.m_alignment);
  glPixelStorei(GL_UNPACK_SKIP_IMAGES, settings.m_skipImages);
  glPixelStorei(GL_UNPACK_SKIP_ROWS, settings.m_skipRows);
  glPixelStorei(GL_UNPACK_SKIP_PIXELS, settings.m_skipPixels);
  glPixelStorei(GL_UNPACK_IMAGE_HEIGHT, settings.m_imageHeight);
  glPixelStorei(GL_UNPACK_ROW_LENGTH, settings.m_rowLength);
  glPixelStorei(GL_UNPACK_LSB_FIRST, settings.m_lsbFirst);
  glPixelStorei(GL_UNPACK_SWAP_BYTES, settings.m_swapBytes);
}
