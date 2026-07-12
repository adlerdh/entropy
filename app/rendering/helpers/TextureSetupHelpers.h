#pragma once

#include "rendering/TextureLayout.h"

#include <glm/vec3.hpp>

#include <optional>
#include <string>
#include <vector>

namespace entropy::rendering::texture_setup
{

/**
 * @brief OpenGL texture size limits needed to decide how an image can be uploaded.
 */
struct TextureLimits
{
  int maxTextureSize = 0;        //!< GL_MAX_TEXTURE_SIZE, used for 1D/2D width and height.
  int max3DTextureSize = 0;      //!< GL_MAX_3D_TEXTURE_SIZE, used for each 3D texture dimension.
  int maxArrayTextureLayers = 0; //!< GL_MAX_ARRAY_TEXTURE_LAYERS, logged for diagnostics.
};

/**
 * @brief Texture layout selected for an image plus the dimensions that should be uploaded to OpenGL.
 */
struct TextureUploadLayout
{
  PlanarTextureLayout layout; //!< Texture dimensionality and, for 2D textures, the represented image axes.
  glm::uvec3 uploadSize{1u};  //!< OpenGL upload size; 2D textures use z = 1.
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
 * @brief Return a user-facing explanation for why a texture exceeds the available OpenGL limits.
 */
std::string textureLimitReason(const glm::uvec3& size, const TextureLimits& limits);

} // namespace entropy::rendering::texture_setup
