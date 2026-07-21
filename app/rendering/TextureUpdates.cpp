#include "rendering/Rendering.h"

#include "common/Exception.hpp"
#include "common/MathFuncs.h"
#include "common/Types.h"
#include "common/UuidRange.h"
#include "logic/app/Data.h"
#include "logic/app/ParcellationLabelTable.h"
#include "rendering/RenderData.h"
#include "rendering/TextureLayout.h"
#include "rendering/TextureSetup.h"
#include "rendering/helpers/PipelineHelpers.h"
#include "rendering/utility/gl/GLBufferTexture.h"
#include "rendering/utility/gl/GLBufferTypes.h"
#include "rendering/utility/gl/GLTexture.h"
#include "rendering/utility/gl/GLTextureTypes.h"

#include <glad/glad.h>

#include <spdlog/fmt/ostr.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <optional>
#include <string>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

namespace
{
using namespace uuids;
}

void Rendering::initTextures()
{
  m_appData.renderData().m_labelBufferTextures = createLabelColorTableTextures(m_appData);

  if (m_appData.renderData().m_labelBufferTextures.empty()) {
    spdlog::critical("No label buffer textures loaded");
    throwDebug("No label buffer textures loaded");
  }

  m_appData.renderData().m_colormapTextures = createImageColorMapTextures(m_appData);

  if (m_appData.renderData().m_colormapTextures.empty()) {
    spdlog::critical("No image color map textures loaded");
    throwDebug("No image color map textures loaded");
  }

  const std::vector<uuid> imageUidsOfCreatedTextures = createImageTextures(m_appData, m_appData.imageUidsOrdered());

  if (imageUidsOfCreatedTextures.size() != m_appData.numImages()) {
    spdlog::error("Not all image textures were created");
  }

  const std::vector<uuid> defUidsOfCreatedTextures = createImageTextures(m_appData, m_appData.defUidsOrdered());
  if (defUidsOfCreatedTextures.size() != m_appData.numDefs()) {
    spdlog::error("Not all warp field textures were created");
  }

  std::vector<uuid> projectionUids;
  for (const uuid& imageUid : m_appData.imageUidsOrdered()) {
    const uuid effectiveImageUid = m_appData.effectiveImageUidForRendering(imageUid);
    if (effectiveImageUid == imageUid) {
      continue;
    }
    if (
      std::find(projectionUids.begin(), projectionUids.end(), effectiveImageUid) == projectionUids.end() &&
      m_appData.renderData().m_imageTextures.find(effectiveImageUid) == m_appData.renderData().m_imageTextures.end())
    {
      projectionUids.push_back(effectiveImageUid);
    }
  }
  if (!projectionUids.empty()) {
    createImageTextures(m_appData, projectionUids);
  }

  const std::vector<uuid> segUidsOfCreatedTextures = createSegTextures(m_appData, m_appData.segUidsOrdered());

  if (segUidsOfCreatedTextures.size() != m_appData.numSegs()) {
    spdlog::error("Not all segmentation textures were created");
  }

  m_appData.renderData().m_distanceMapTextures = createDistanceMapTextures(m_appData);

  m_isAppDoneLoadingImages = true;
}

bool Rendering::createLabelColorTableTexture(const uuid& labelTableUid)
{
  // static const glm::vec4 sk_border{ 0.0f, 0.0f, 0.0f, 0.0f };

  const auto* table = m_appData.labelTable(labelTableUid);
  if (!table) {
    spdlog::warn("Label table {} is invalid", labelTableUid);
    return false;
  }

  int maxBufTexSize = 0;
  glGetIntegerv(GL_MAX_TEXTURE_BUFFER_SIZE, &maxBufTexSize);

  if (table->numColorBytes_RGBA_U8() > static_cast<size_t>(maxBufTexSize)) {
    spdlog::error(
      "Number of bytes ({}) in label color table {} exceeds "
      "maximum buffer texture size of {} bytes",
      table->numColorBytes_RGBA_U8(),
      labelTableUid,
      maxBufTexSize);
    return false;
  }

  auto it = m_appData.renderData().m_labelBufferTextures.emplace(
    std::piecewise_construct,
    std::forward_as_tuple(labelTableUid),
    std::forward_as_tuple(ParcellationLabelTable::bufferTextureFormat_RGBA_U8(), BufferUsagePattern::StaticDraw));

  if (!it.second) {
    return false;
  }

  GLBufferTexture& T = it.first->second;
  T.generate();
  T.allocate(table->numColorBytes_RGBA_U8(), table->colorData_RGBA_nonpremult_U8());

  spdlog::debug("Generated buffer texture for label color table {}", labelTableUid);
  return true;
}

bool Rendering::removeSegTexture(const uuid& segUid)
{
  const auto* seg = m_appData.seg(segUid);
  if (!seg) {
    spdlog::warn("Segmentation {} is invalid", segUid);
    return false;
  }

  auto it = m_appData.renderData().m_segTextures.find(segUid);

  if (std::end(m_appData.renderData().m_segTextures) == it) {
    spdlog::warn("Texture for segmentation {} does not exist and cannot be removed", segUid);
    return false;
  }

  m_appData.renderData().m_segTextures.erase(it);
  m_appData.renderData().m_segTextureLayouts.erase(segUid);
  return true;
}

void Rendering::updateSegTexture(
  const uuid& segUid,
  const ComponentType& compType,
  const glm::uvec3& startOffsetVoxel,
  const glm::uvec3& sizeInVoxels,
  const void* data)
{
  // Load seg data into first mipmap level
  static constexpr GLint sk_mipmapLevel = 0;

  auto it = m_appData.renderData().m_segTextures.find(segUid);
  if (std::end(m_appData.renderData().m_segTextures) == it) {
    spdlog::error("Cannot update segmentation {}: texture not found.", segUid);
    return;
  }

  const auto* seg = m_appData.seg(segUid);
  if (!seg) {
    spdlog::warn("Segmentation {} is invalid", segUid);
    return;
  }

  const auto layoutIt = m_appData.renderData().m_segTextureLayouts.find(segUid);
  if (
    layoutIt != std::end(m_appData.renderData().m_segTextureLayouts) &&
    RenderData::TextureDimension::Texture2D == layoutIt->second.dimension)
  {
    m_appData.renderData().m_segTextures.erase(segUid);
    m_appData.renderData().m_segTextureLayouts.erase(segUid);
    createSegTextures(m_appData, uuid_range_t{segUid});
    return;
  }

  GLTexture& T = it->second;
  T.setSubData(
    sk_mipmapLevel,
    startOffsetVoxel,
    sizeInVoxels,
    GLTexture::getBufferPixelRedFormat(compType),
    GLTexture::getBufferPixelDataType(compType),
    data);
}

void Rendering::updateSegTextureWithInt64Data(
  const uuid& segUid,
  const ComponentType& compType,
  const glm::uvec3& startOffsetVoxel,
  const glm::uvec3& sizeInVoxels,
  const int64_t* data)
{
  if (!data) {
    spdlog::error("Null segmentation texture data pointer");
    return;
  }

  if (!isValidSegmentationComponentType(compType)) {
    spdlog::error(
      "Unable to update segmentation texture using buffer with invalid component type {}",
      componentTypeString(compType));
    return;
  }

  const size_t N =
    static_cast<size_t>(sizeInVoxels.x) * static_cast<size_t>(sizeInVoxels.y) * static_cast<size_t>(sizeInVoxels.z);

  switch (compType) {
    case ComponentType::UInt8: {
      std::vector<uint8_t> castData(N);
      std::transform(data, data + N, castData.begin(), [](int64_t value) { return static_cast<uint8_t>(value); });
      return updateSegTexture(
        segUid,
        compType,
        startOffsetVoxel,
        sizeInVoxels,
        static_cast<const void*>(castData.data()));
    }
    case ComponentType::UInt16: {
      std::vector<uint16_t> castData(N);
      std::transform(data, data + N, castData.begin(), [](int64_t value) { return static_cast<uint16_t>(value); });
      return updateSegTexture(
        segUid,
        compType,
        startOffsetVoxel,
        sizeInVoxels,
        static_cast<const void*>(castData.data()));
    }
    case ComponentType::UInt32: {
      std::vector<uint32_t> castData(N);
      std::transform(data, data + N, castData.begin(), [](int64_t value) { return static_cast<uint32_t>(value); });
      return updateSegTexture(
        segUid,
        compType,
        startOffsetVoxel,
        sizeInVoxels,
        static_cast<const void*>(castData.data()));
    }
    default: {
      return;
    }
  }
}

void Rendering::updateBrushPreviewTexture(
  const uuid& imageUid,
  const uuid& segUid,
  const ComponentType& compType,
  const glm::uvec3& sizeInVoxels,
  const glm::mat4& voxel_T_world,
  const glm::vec4& color,
  bool allowFill,
  const int64_t* data)
{
  if (!data || glm::any(glm::lessThanEqual(sizeInVoxels, glm::uvec3{0}))) {
    return;
  }

  auto& preview = m_appData.renderData().m_brushPreviews[imageUid];
  const bool fitsInCapacity = glm::all(glm::lessThanEqual(sizeInVoxels, preview.textureCapacity));
  if (!preview.texture || !fitsInCapacity) {
    preview.textureCapacity = rendering::growBrushPreviewCapacity(preview.textureCapacity, sizeInVoxels);
  }

  const glm::uvec3 uploadSize = preview.textureCapacity;
  const bool needsNewTexture =
    !preview.texture || preview.texture->size() != uploadSize || preview.componentType != compType;
  preview.visible = true;
  preview.segUid = segUid;
  preview.imageUid = imageUid;
  preview.componentType = compType;
  preview.size = sizeInVoxels;
  preview.voxel_T_world = voxel_T_world;
  preview.texture_T_world =
    glm::mat4{math::computeImagePixelToTextureTransformation(glm::u64vec3{uploadSize})} * voxel_T_world;
  preview.color = color;
  preview.allowFill = allowFill;

  GLTexture::PixelStoreSettings pixelStoreSettings;
  pixelStoreSettings.m_alignment = 1;

  auto uploadPreviewData =
    [&preview, &pixelStoreSettings, &uploadSize, &compType, needsNewTexture](const void* uploadData) {
      if (needsNewTexture) {
        preview.texture
          .emplace(tex::Target::Texture3D, GLTexture::MultisampleSettings(), pixelStoreSettings, pixelStoreSettings);

        GLTexture& texture = *preview.texture;
        texture.generate();
        texture.setMinificationFilter(tex::MinificationFilter::Nearest);
        texture.setMagnificationFilter(tex::MagnificationFilter::Nearest);
        texture.setBorderColor(glm::vec4{0.0f});
        texture.setWrapMode(tex::WrapMode::ClampToBorder);
        texture.setAutoGenerateMipmaps(false);
        texture.setSize(uploadSize);
        texture.setData(
          0,
          GLTexture::getSizedInternalRedFormat(compType),
          GLTexture::getBufferPixelRedFormat(compType),
          GLTexture::getBufferPixelDataType(compType),
          uploadData);
      }
      else {
        preview.texture->setSubData(
          0,
          glm::uvec3{0},
          uploadSize,
          GLTexture::getBufferPixelRedFormat(compType),
          GLTexture::getBufferPixelDataType(compType),
          uploadData);
      }
    };

  switch (compType) {
    case ComponentType::UInt8: {
      uploadPreviewData(rendering::prepareBrushPreviewUploadData(preview.dataU8, data, sizeInVoxels, uploadSize));
      break;
    }
    case ComponentType::UInt16: {
      uploadPreviewData(rendering::prepareBrushPreviewUploadData(preview.dataU16, data, sizeInVoxels, uploadSize));
      break;
    }
    case ComponentType::UInt32: {
      uploadPreviewData(rendering::prepareBrushPreviewUploadData(preview.dataU32, data, sizeInVoxels, uploadSize));
      break;
    }
    default: {
      return;
    }
  }
}

void Rendering::clearBrushPreviewTextures()
{
  m_appData.renderData().m_brushPreviews.clear();
}

void Rendering::hideBrushPreviewTextures()
{
  for (auto& entry : m_appData.renderData().m_brushPreviews) {
    entry.second.visible = false;
  }
}

void Rendering::updateImageTexture(
  const uuid& imageUid,
  uint32_t component,
  const ComponentType& compType,
  const glm::uvec3& startOffsetVoxel,
  const glm::uvec3& sizeInVoxels,
  const void* data)
{
  // Load data into first mipmap level
  static constexpr GLint sk_mipmapLevel = 0;

  auto it = m_appData.renderData().m_imageTextures.find(imageUid);
  if (std::end(m_appData.renderData().m_imageTextures) == it) {
    spdlog::error("Cannot update image {}: texture not found.", imageUid);
    return;
  }

  std::vector<GLTexture>& T = it->second;
  if (component >= T.size()) {
    spdlog::error("Cannot update invalid component {} of image {}", component, imageUid);
    return;
  }

  const auto* img = m_appData.image(imageUid);
  if (!img) {
    spdlog::warn("Segmentation {} is invalid", imageUid);
    return;
  }

  const auto layoutIt = m_appData.renderData().m_imageTextureLayouts.find(imageUid);
  if (
    layoutIt != std::end(m_appData.renderData().m_imageTextureLayouts) &&
    RenderData::TextureDimension::Texture2D == layoutIt->second.dimension)
  {
    m_appData.renderData().m_imageTextures.erase(imageUid);
    m_appData.renderData().m_imageTextureLayouts.erase(imageUid);
    createImageTextures(m_appData, uuid_range_t{imageUid});
    return;
  }

  T.at(component).setSubData(
    sk_mipmapLevel,
    startOffsetVoxel,
    sizeInVoxels,
    GLTexture::getBufferPixelRedFormat(compType),
    GLTexture::getBufferPixelDataType(compType),
    data);
}
