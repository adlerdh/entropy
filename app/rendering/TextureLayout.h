#pragma once

#include <glm/vec2.hpp>

namespace rendering
{

/// OpenGL texture target used for an uploaded image or segmentation texture.
enum class TextureDimension
{
  Texture3D, //!< Texture is uploaded as GL_TEXTURE_3D.
  Texture2D  //!< Planar texture is uploaded as GL_TEXTURE_2D because it exceeds 3D texture limits.
};

/**
 * @brief Texture dimensionality and axis mapping for images that may be uploaded as 2D or 3D textures.
 *
 * For 3D textures, `axes` is ignored. For 2D textures, `axes` stores the two image axes represented by the texture
 * s/t coordinates, such as (0, 1) for an XY plane or (1, 2) for a YZ plane.
 */
struct PlanarTextureLayout
{
  TextureDimension dimension = TextureDimension::Texture3D;
  glm::ivec2 axes{0, 1};
};

} // namespace rendering
