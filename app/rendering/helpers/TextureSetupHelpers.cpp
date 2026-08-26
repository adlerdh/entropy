#include "rendering/helpers/TextureSetupHelpers.h"

#include <cstdint>

namespace rendering::texture_setup
{

std::vector<int> nonSingletonAxes(const glm::uvec3& size)
{
  std::vector<int> axes;
  axes.reserve(3);
  for (int axis = 0; axis < 3; ++axis) {
    if (size[axis] > 1u) {
      axes.push_back(axis);
    }
  }
  return axes;
}

bool fitsMax3DTextureSize(const glm::uvec3& size, const TextureLimits& limits)
{
  if (limits.max3DTextureSize <= 0 || size.x == 0u || size.y == 0u || size.z == 0u) {
    return false;
  }
  return size.x <= static_cast<uint32_t>(limits.max3DTextureSize) &&
         size.y <= static_cast<uint32_t>(limits.max3DTextureSize) &&
         size.z <= static_cast<uint32_t>(limits.max3DTextureSize);
}

bool fitsMax2DTextureSize(const glm::uvec2& size, const TextureLimits& limits)
{
  if (limits.maxTextureSize <= 0 || size.x == 0u || size.y == 0u) {
    return false;
  }
  return size.x <= static_cast<uint32_t>(limits.maxTextureSize) &&
         size.y <= static_cast<uint32_t>(limits.maxTextureSize);
}

std::optional<TextureUploadRegion> textureUploadRegion(
  const PlanarTextureLayout& layout,
  const glm::uvec3& imageSize,
  const glm::uvec3& imageOffset,
  const glm::uvec3& imageRegionSize)
{
  if (imageSize.x == 0u || imageSize.y == 0u || imageSize.z == 0u) {
    return std::nullopt;
  }

  for (int axis = 0; axis < 3; ++axis) {
    if (
      imageRegionSize[axis] == 0u || imageOffset[axis] > imageSize[axis] ||
      imageRegionSize[axis] > imageSize[axis] - imageOffset[axis])
    {
      return std::nullopt;
    }
  }

  if (TextureDimension::Texture3D == layout.dimension) {
    return TextureUploadRegion{.offset = imageOffset, .size = imageRegionSize};
  }

  const int axis0 = layout.axes.x;
  const int axis1 = layout.axes.y;
  if (axis0 < 0 || axis0 > 2 || axis1 < 0 || axis1 > 2 || axis0 >= axis1) {
    return std::nullopt;
  }

  const int omittedAxis = 3 - axis0 - axis1;
  if (imageSize[omittedAxis] != 1u || imageOffset[omittedAxis] != 0u || imageRegionSize[omittedAxis] != 1u) {
    return std::nullopt;
  }

  return TextureUploadRegion{
    .offset = {imageOffset[axis0], imageOffset[axis1], 0u},
    .size = {imageRegionSize[axis0], imageRegionSize[axis1], 1u}};
}

std::optional<TextureUploadLayout> textureUploadLayoutForImage(const glm::uvec3& size, const TextureLimits& limits)
{
  if (size.x == 0u || size.y == 0u || size.z == 0u || limits.maxTextureSize <= 0 || limits.max3DTextureSize <= 0) {
    return std::nullopt;
  }

  if (fitsMax3DTextureSize(size, limits)) {
    TextureUploadLayout uploadLayout;
    uploadLayout.layout.dimension = TextureDimension::Texture3D;
    uploadLayout.uploadSize = size;
    return uploadLayout;
  }

  const std::vector<int> axes = nonSingletonAxes(size);
  if (axes.size() != 2u) {
    return std::nullopt;
  }

  const glm::uvec2 size2D{size[axes[0]], size[axes[1]]};
  if (!fitsMax2DTextureSize(size2D, limits)) {
    return std::nullopt;
  }

  TextureUploadLayout uploadLayout;
  uploadLayout.layout.dimension = TextureDimension::Texture2D;
  uploadLayout.layout.axes = glm::ivec2{axes[0], axes[1]};
  uploadLayout.uploadSize = glm::uvec3{size2D.x, size2D.y, 1u};
  return uploadLayout;
}

std::string textureLimitReason(const glm::uvec3& size, const TextureLimits& limits)
{
  if (size.x == 0u || size.y == 0u || size.z == 0u) {
    return "Image texture dimensions must all be greater than zero.";
  }
  if (limits.maxTextureSize <= 0 || limits.max3DTextureSize <= 0) {
    return "The OpenGL context did not report valid 2D and 3D texture size limits.";
  }
  const std::vector<int> axes = nonSingletonAxes(size);
  if (axes.size() == 2u) {
    return "This OpenGL context reports GL_MAX_3D_TEXTURE_SIZE = " + std::to_string(limits.max3DTextureSize) +
           " and GL_MAX_TEXTURE_SIZE = " + std::to_string(limits.maxTextureSize) +
           ". The image is planar, but its 2D dimensions still exceed the 2D texture limit.";
  }
  return "This OpenGL context reports GL_MAX_3D_TEXTURE_SIZE = " + std::to_string(limits.max3DTextureSize) +
         ", meaning each 3D texture dimension must be less than or equal to that value.";
}

} // namespace rendering::texture_setup
