#include "rendering/RenderResources.h"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <array>
#include <cstddef>

namespace rendering
{
namespace
{
constexpr int k_numQuadVerts = 4;
constexpr int k_numQuadPositionComponents = 2;
constexpr int k_byteOffset = 0;
constexpr int k_indexOffset = 0;

const std::array<float, static_cast<std::size_t>(k_numQuadVerts* k_numQuadPositionComponents)> k_clipPositions =
  {-1.0f, -1.0f, 1.0f, -1.0f, -1.0f, 1.0f, 1.0f, 1.0f};
const std::array<uint32_t, k_numQuadVerts> k_indices = {0, 1, 2, 3};

GLTexture createBlankRgbaTexture(tex::Target target, uint8_t value)
{
  constexpr ComponentType componentType = ComponentType::UInt8;
  const std::array<uint8_t, 4> data{value, value, value, value};
  constexpr GLint mipmapLevel = 0;
  constexpr GLint alignment = 4;
  const glm::uvec3 size{1, 1, 1};

  GLTexture::PixelStoreSettings pixelStoreSettings;
  pixelStoreSettings.m_alignment = alignment;
  GLTexture texture(target, GLTexture::MultisampleSettings(), pixelStoreSettings, pixelStoreSettings);
  texture.generate();
  texture.setMinificationFilter(tex::MinificationFilter::Nearest);
  texture.setMagnificationFilter(tex::MagnificationFilter::Nearest);
  texture.setWrapMode(tex::WrapMode::ClampToEdge);
  texture.setAutoGenerateMipmaps(false);
  texture.setSize(size);
  texture.setData(
    mipmapLevel,
    GLTexture::getSizedInternalNormalizedRGBAFormat(componentType),
    GLTexture::getBufferPixelNormalizedRGBAFormat(componentType),
    GLTexture::getBufferPixelDataType(componentType),
    data.data());
  return texture;
}

GLTexture createBlankSegTexture(tex::Target target)
{
  constexpr ComponentType componentType = ComponentType::UInt8;
  constexpr uint8_t zero = 0;
  constexpr GLint mipmapLevel = 0;
  constexpr GLint alignment = 1;
  const glm::uvec3 size{1, 1, 1};

  GLTexture::PixelStoreSettings pixelStoreSettings;
  pixelStoreSettings.m_alignment = alignment;
  GLTexture texture(target, GLTexture::MultisampleSettings(), pixelStoreSettings, pixelStoreSettings);
  texture.generate();
  texture.setMinificationFilter(tex::MinificationFilter::Nearest);
  texture.setMagnificationFilter(tex::MagnificationFilter::Nearest);
  texture.setWrapMode(tex::WrapMode::ClampToEdge);
  texture.setAutoGenerateMipmaps(false);
  texture.setSize(size);
  texture.setData(
    mipmapLevel,
    GLTexture::getSizedInternalRedFormat(componentType),
    GLTexture::getBufferPixelRedFormat(componentType),
    GLTexture::getBufferPixelDataType(componentType),
    &zero);
  return texture;
}
} // namespace

RenderResources::RenderResources()
  : m_blankImageBlackTransparentTexture(createBlankRgbaTexture(tex::Target::Texture3D, 0))
  , m_blankImageBlackTransparentTexture2D(createBlankRgbaTexture(tex::Target::Texture2D, 0))
  , m_blankImageWhiteOpaqueTexture(createBlankRgbaTexture(tex::Target::Texture3D, 255))
  , m_blankSegTexture(createBlankSegTexture(tex::Target::Texture3D))
  , m_blankSegTexture2D(createBlankSegTexture(tex::Target::Texture2D))
  , m_blankDistMapTexture(createBlankRgbaTexture(tex::Target::Texture3D, 0))
{
}

void RenderResources::clearLoadedData()
{
  m_imageTextures.clear();
  m_imageTextureLayouts.clear();
  m_distanceMapTextures.clear();
  m_segTextures.clear();
  m_segTextureLayouts.clear();
  m_brushPreviews.clear();
  m_labelBufferTextures.clear();
  m_colormapTextures.clear();
}

void RenderResources::removeImage(const uuids::uuid& imageUid)
{
  m_imageTextures.erase(imageUid);
  m_imageTextureLayouts.erase(imageUid);
  m_distanceMapTextures.erase(imageUid);
  m_brushPreviews.erase(imageUid);
}

void RenderResources::removeSegmentation(const uuids::uuid& segmentationUid)
{
  m_segTextures.erase(segmentationUid);
  m_segTextureLayouts.erase(segmentationUid);
  std::erase_if(m_brushPreviews, [&segmentationUid](const auto& entry) {
    return entry.second.segUid == segmentationUid;
  });
}

bool RenderResources::hasSegmentation(const uuids::uuid& segmentationUid) const noexcept
{
  return m_segTextures.contains(segmentationUid);
}

RenderResources::Quad::Quad()
  : m_positionsInfo(
      BufferComponentType::Float,
      BufferNormalizeValues::False,
      k_numQuadPositionComponents,
      k_numQuadPositionComponents * sizeof(float),
      k_byteOffset,
      k_numQuadVerts)
  , m_indicesInfo(IndexType::UInt32, PrimitiveMode::TriangleStrip, k_numQuadVerts, k_indexOffset)
  , m_positionsObject(BufferType::VertexArray, BufferUsagePattern::StaticDraw)
  , m_indicesObject(BufferType::Index, BufferUsagePattern::StaticDraw)
  , m_vaoParams(m_indicesInfo)
{
  constexpr GLuint positionIndex = 0;
  m_positionsObject.generate();
  m_indicesObject.generate();
  m_positionsObject.allocate(k_clipPositions.size() * sizeof(float), k_clipPositions.data());
  m_indicesObject.allocate(k_indices.size() * sizeof(uint32_t), k_indices.data());

  m_vao.generate();
  m_vao.bind();
  m_indicesObject.bind();
  m_positionsObject.bind();
  m_vao.setAttributeBuffer(positionIndex, m_positionsInfo);
  m_vao.enableVertexAttribute(positionIndex);
  m_positionsObject.unbind();
  GLVertexArrayObject::unbind();
  spdlog::debug("Created image quad vertex array object");
}

RenderResources::Circle::Circle()
  : m_positionsInfo(
      BufferComponentType::Float,
      BufferNormalizeValues::False,
      k_numQuadPositionComponents,
      k_numQuadPositionComponents * sizeof(float),
      k_byteOffset,
      k_numQuadVerts)
  , m_indicesInfo(IndexType::UInt32, PrimitiveMode::TriangleStrip, k_numQuadVerts, k_indexOffset)
  , m_positionsObject(BufferType::VertexArray, BufferUsagePattern::StaticDraw)
  , m_indicesObject(BufferType::Index, BufferUsagePattern::StaticDraw)
  , m_vaoParams(m_indicesInfo)
{
  constexpr GLuint positionIndex = 0;
  m_positionsObject.generate();
  m_indicesObject.generate();
  m_positionsObject.allocate(k_clipPositions.size() * sizeof(float), k_clipPositions.data());
  m_indicesObject.allocate(k_indices.size() * sizeof(uint32_t), k_indices.data());

  m_vao.generate();
  m_vao.bind();
  m_indicesObject.bind();
  m_positionsObject.bind();
  m_vao.setAttributeBuffer(positionIndex, m_positionsInfo);
  m_vao.enableVertexAttribute(positionIndex);
  m_positionsObject.unbind();
  GLVertexArrayObject::unbind();
  spdlog::debug("Created image circle vertex array object");
}

} // namespace rendering
