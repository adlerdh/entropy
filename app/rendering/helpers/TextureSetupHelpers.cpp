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
  return size.x <= static_cast<uint32_t>(limits.max3DTextureSize) &&
         size.y <= static_cast<uint32_t>(limits.max3DTextureSize) &&
         size.z <= static_cast<uint32_t>(limits.max3DTextureSize);
}

bool fitsMax2DTextureSize(const glm::uvec2& size, const TextureLimits& limits)
{
  return size.x <= static_cast<uint32_t>(limits.maxTextureSize) &&
         size.y <= static_cast<uint32_t>(limits.maxTextureSize);
}

std::optional<TextureUploadLayout> textureUploadLayoutForImage(const glm::uvec3& size, const TextureLimits& limits)
{
  if (fitsMax3DTextureSize(size, limits)) {
    return TextureUploadLayout{
      .layout = PlanarTextureLayout{.dimension = TextureDimension::Texture3D},
      .uploadSize = size};
  }

  const std::vector<int> axes = nonSingletonAxes(size);
  if (axes.size() != 2u) {
    return std::nullopt;
  }

  const glm::uvec2 size2D{size[axes[0]], size[axes[1]]};
  if (!fitsMax2DTextureSize(size2D, limits)) {
    return std::nullopt;
  }

  return TextureUploadLayout{
    .layout = PlanarTextureLayout{.dimension = TextureDimension::Texture2D, .axes = glm::ivec2{axes[0], axes[1]}},
    .uploadSize = glm::uvec3{size2D.x, size2D.y, 1u}};
}

std::string textureLimitReason(const glm::uvec3& size, const TextureLimits& limits)
{
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
