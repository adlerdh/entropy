#pragma once

#include "rendering/TextureLayout.h"

#include "common/Types.h"

#include <glm/mat3x3.hpp>
#include <glm/vec3.hpp>

#include <optional>
#include <span>
#include <string>
#include <vector>

namespace rendering::texture_setup
{

/** Return native image-value thresholds for a global pair of foreground percentiles. */
std::pair<double, double>
distanceMapForegroundThresholds(const ComponentStats& statistics, float lowerPercentile, float upperPercentile);

/**
 * @brief OpenGL texture size limits needed to decide how an image can be uploaded.
 */
struct TextureLimits
{
  int maxTextureSize = 0;        //!< GL_MAX_TEXTURE_SIZE, used for 1D/2D width and height
  int max3DTextureSize = 0;      //!< GL_MAX_3D_TEXTURE_SIZE, used for each 3D texture dimension
  int maxArrayTextureLayers = 0; //!< GL_MAX_ARRAY_TEXTURE_LAYERS, logged for diagnostics
};

/**
 * @brief Texture layout selected for an image plus the dimensions that should be uploaded to OpenGL.
 */
struct TextureUploadLayout
{
  PlanarTextureLayout layout; //!< Texture dimensionality and, for 2D textures, the represented image axes
  glm::uvec3 uploadSize{1u};  //!< OpenGL upload size; 2D textures use z = 1
};

/** @brief A voxel update mapped into the coordinate system of an uploaded OpenGL texture. */
struct TextureUploadRegion
{
  glm::uvec3 offset{0u}; //!< Texture-space update origin; z is zero for GL_TEXTURE_2D
  glm::uvec3 size{0u};   //!< Texture-space update extent; z is one for GL_TEXTURE_2D
};

/** @brief Image geometry needed to compare normalized texture-coordinate domains. */
struct TextureGeometry
{
  glm::uvec3 dimensions{0u};
  glm::vec3 spacing{0.0f};
  glm::vec3 origin{0.0f};
  glm::mat3 directions{1.0f};
};

/**
 * @brief Return axes whose image dimension is greater than one.
 */
std::vector<int> nonSingletonAxes(const glm::uvec3& size);

/**
 * @brief Return whether all dimensions fit within GL_MAX_3D_TEXTURE_SIZE.
 */
bool fitsMax3DTextureSize(const glm::uvec3& size, const TextureLimits& limits);

/**
 * @brief Return whether both dimensions fit within GL_MAX_TEXTURE_SIZE.
 */
bool fitsMax2DTextureSize(const glm::uvec2& size, const TextureLimits& limits);

/**
 * @brief Choose the OpenGL texture target and upload size for an image or segmentation.
 *
 * Volumes that fit within the reported 3D texture limit use GL_TEXTURE_3D. Planar images that exceed the 3D limit may
 * fall back to GL_TEXTURE_2D when their two non-singleton axes fit within the reported 2D texture limit.
 */
std::optional<TextureUploadLayout> textureUploadLayoutForImage(const glm::uvec3& size, const TextureLimits& limits);

/**
 * @brief Map and validate an image-space subregion for its selected texture layout.
 *
 * Three-dimensional layouts preserve the image offset and extent. For planar layouts, the omitted image axis must be
 * singleton and is removed from the returned texture coordinates. The returned region can be passed directly to
 * GLTexture::setSubData.
 */
std::optional<TextureUploadRegion> textureUploadRegion(
  const PlanarTextureLayout& layout,
  const glm::uvec3& imageSize,
  const glm::uvec3& imageOffset,
  const glm::uvec3& imageRegionSize);

/**
 * @brief Explain why a derived texture does not cover the same physical domain as its source.
 *
 * Dimensions may differ. This permits intentionally downsampled derived images as long as their
 * voxel-edge bounds and directions remain aligned with the source image.
 */
std::optional<std::string> textureDomainMismatchReason(const TextureGeometry& source, const TextureGeometry& derived);

/**
 * @brief Return whether foreground-mask distances conservatively accelerate all active isovalues.
 *
 * Outside the foreground interval, a distance to that interval does not bound the distance to an
 * unrelated isosurface and must not be used for empty-space skipping.
 */
bool distanceMapSupportsIsovalues(double foregroundLow, double foregroundHigh, std::span<const float> isovalues);

/**
 * @brief Return a user-facing explanation for why a texture exceeds the available OpenGL limits.
 */
std::string textureLimitReason(const glm::uvec3& size, const TextureLimits& limits);

} // namespace rendering::texture_setup
