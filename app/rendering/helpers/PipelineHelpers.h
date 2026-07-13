#pragma once

#include "rendering/common/ShaderType.h"
#include "rendering/TextureLayout.h"

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <uuid.h>

#include <algorithm>
#include <cstdint>
#include <limits>
#include <list>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

class GLShaderProgram;

namespace rendering
{

/**
 * @brief Replace every occurrence of each placeholder string in source text.
 *
 * Replacements are applied in the iteration order of the supplied map. This is intended for shader source generation,
 * where placeholder keys are unique sentinel tokens rather than overlapping natural-language text.
 */
std::string replacePlaceholders(
  const std::string& source,
  const std::unordered_map<std::string, std::string>& placeholdersToStringMap);

/**
 * @brief Shader source snippets used to adapt shader placeholders for 2D fallback textures.
 */
struct TextureLookupReplacementSources
{
  std::string linear3D;
  std::string linear2D;
  std::string floatingPointLinear3D;
  std::string floatingPointLinear2D;
  std::string cubic3D;
  std::string cubic2D;
  std::string uintLinear2D;
};

/**
 * @brief Return shader placeholder replacements for the requested texture dimension.
 *
 * The 3D path only sets sampler placeholder types. The 2D fallback path also swaps texture lookup helpers to
 * dimension-specific implementations while preserving all unrelated placeholders.
 */
std::unordered_map<std::string, std::string> shaderReplacementsForTextureDimension(
  const std::unordered_map<std::string, std::string>& replacements,
  TextureDimension dimension,
  const TextureLookupReplacementSources& lookupSources);

/**
 * @brief Grow a one-dimensional brush-preview texture capacity enough to hold a required size.
 *
 * Existing capacity is reused when possible. Non-zero capacity grows by 50% and is clamped to uint32_t max.
 */
uint32_t growBrushPreviewCapacity(uint32_t current, uint32_t required);

/**
 * @brief Grow a three-dimensional brush-preview texture capacity enough to hold a required size.
 */
glm::uvec3 growBrushPreviewCapacity(const glm::uvec3& current, const glm::uvec3& required);

/**
 * @brief Return a cached texture layout for an optional image id, or the default 3D layout when absent.
 */
PlanarTextureLayout textureLayoutOrDefault(
  const std::unordered_map<uuids::uuid, PlanarTextureLayout>& layouts,
  const std::optional<uuids::uuid>& uid);

/**
 * @brief Return true when a texture layout represents a 2D fallback texture.
 */
bool isTexture2D(const PlanarTextureLayout& layout);

/**
 * @brief Return the image axes represented by a shader program texture slot.
 *
 * 3D textures use the conventional x/y axes because the value is ignored by 3D shader variants.
 */
glm::ivec2 textureAxesForProgramSlot(const PlanarTextureLayout& layout);

/**
 * @brief Return true when an image texture layout can be used by the 3D raycast renderer.
 *
 * Missing layout entries are treated as legacy/default 3D textures.
 */
bool imageHasRaycastableTextureLayout(
  const std::unordered_map<uuids::uuid, PlanarTextureLayout>& textureLayouts,
  const uuids::uuid& imageUid);

/**
 * @brief Filter a list of image ids down to the images whose uploaded texture layout supports 3D raycasting.
 *
 * Missing layout entries are treated as legacy/default 3D textures.
 */
std::list<uuids::uuid> raycastableImageUids(
  const std::list<uuids::uuid>& imageUids,
  const std::unordered_map<uuids::uuid, PlanarTextureLayout>& textureLayouts);

/**
 * @brief Select the matching 2D or 3D shader program for an uploaded texture layout.
 */
GLShaderProgram& shaderProgramForTextureDimension(
  std::unordered_map<ShaderProgramType, std::unique_ptr<GLShaderProgram>>& shaderPrograms3D,
  std::unordered_map<ShaderProgramType, std::unique_ptr<GLShaderProgram>>& shaderPrograms2D,
  ShaderProgramType shaderType,
  TextureDimension dimension);

/**
 * @brief Set shader uniforms that describe the source image axes used by one or two 2D fallback textures.
 */
void setTexture2DAxesUniforms(
  GLShaderProgram& program,
  const PlanarTextureLayout& slot0,
  const PlanarTextureLayout& slot1 = {});

/**
 * @brief Copy a dense brush-preview mask into a buffer sized to the preview texture capacity.
 *
 * Source data is tightly packed to sizeInVoxels. Destination data is tightly packed to textureCapacity and zero-filled
 * outside the uploaded preview region. The returned pointer is stable until uploadData is modified again.
 */
template<typename ValueType>
const void* prepareBrushPreviewUploadData(
  std::vector<ValueType>& uploadData,
  const int64_t* data,
  const glm::uvec3& sizeInVoxels,
  const glm::uvec3& textureCapacity)
{
  const size_t numCapacityVoxels = static_cast<size_t>(textureCapacity.x) * static_cast<size_t>(textureCapacity.y) *
                                   static_cast<size_t>(textureCapacity.z);

  uploadData.resize(numCapacityVoxels);
  std::fill(uploadData.begin(), uploadData.end(), ValueType{0});

  for (uint32_t z = 0; z < sizeInVoxels.z; ++z) {
    for (uint32_t y = 0; y < sizeInVoxels.y; ++y) {
      for (uint32_t x = 0; x < sizeInVoxels.x; ++x) {
        const size_t srcIndex =
          static_cast<size_t>(x) +
          static_cast<size_t>(sizeInVoxels.x) *
            (static_cast<size_t>(y) + static_cast<size_t>(sizeInVoxels.y) * static_cast<size_t>(z));
        const size_t dstIndex =
          static_cast<size_t>(x) +
          static_cast<size_t>(textureCapacity.x) *
            (static_cast<size_t>(y) + static_cast<size_t>(textureCapacity.y) * static_cast<size_t>(z));
        uploadData[dstIndex] = static_cast<ValueType>(data[srcIndex]);
      }
    }
  }

  return uploadData.data();
}

} // namespace rendering
