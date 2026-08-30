#include "rendering/helpers/TextureSetupHelpers.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace rendering::texture_setup
{

namespace
{

constexpr float k_geometryTolerance = 1.0e-4f;

bool nearlyEqual(float a, float b)
{
  return std::abs(a - b) <= k_geometryTolerance * std::max({1.0f, std::abs(a), std::abs(b)});
}

bool nearlyEqual(const glm::vec3& a, const glm::vec3& b)
{
  return nearlyEqual(a.x, b.x) && nearlyEqual(a.y, b.y) && nearlyEqual(a.z, b.z);
}

bool validGeometry(const TextureGeometry& geometry)
{
  return geometry.dimensions.x > 0u && geometry.dimensions.y > 0u && geometry.dimensions.z > 0u &&
         geometry.spacing.x > 0.0f && geometry.spacing.y > 0.0f && geometry.spacing.z > 0.0f;
}

glm::vec3 lowerVoxelEdge(const TextureGeometry& geometry)
{
  return geometry.origin - geometry.directions * (0.5f * geometry.spacing);
}

glm::vec3 axisExtent(const TextureGeometry& geometry, int axis)
{
  return geometry.directions[axis] * geometry.spacing[axis] * static_cast<float>(geometry.dimensions[axis]);
}

} // namespace

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

std::optional<std::string> textureDomainMismatchReason(const TextureGeometry& source, const TextureGeometry& derived)
{
  if (!validGeometry(source)) {
    return "the source image has invalid dimensions or spacing";
  }
  if (!validGeometry(derived)) {
    return "the derived image has invalid dimensions or spacing";
  }

  for (int column = 0; column < 3; ++column) {
    if (!nearlyEqual(source.directions[column], derived.directions[column])) {
      return "its directions differ from the source image";
    }
  }

  if (!nearlyEqual(lowerVoxelEdge(source), lowerVoxelEdge(derived))) {
    return "its physical origin does not align with the source image voxel bounds";
  }

  for (int axis = 0; axis < 3; ++axis) {
    if (!nearlyEqual(axisExtent(source, axis), axisExtent(derived, axis))) {
      return "its physical extent differs from the source image";
    }
  }

  return std::nullopt;
}

bool distanceMapSupportsIsovalues(double foregroundLow, double foregroundHigh, std::span<const float> isovalues)
{
  if (
    !std::isfinite(foregroundLow) || !std::isfinite(foregroundHigh) || foregroundLow > foregroundHigh ||
    isovalues.empty())
  {
    return false;
  }
  return std::all_of(isovalues.begin(), isovalues.end(), [foregroundLow, foregroundHigh](float value) {
    return std::isfinite(value) && foregroundLow <= static_cast<double>(value) &&
           static_cast<double>(value) <= foregroundHigh;
  });
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
