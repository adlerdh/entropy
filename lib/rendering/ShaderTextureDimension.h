#pragma once

#include "rendering/TextureLayout.h"

#include <string>
#include <unordered_map>

namespace rendering
{

/**
 * @brief Shader source snippets used to adapt shader templates for 2D fallback textures.
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
 * @brief Return shader substitutions for the requested texture dimension.
 *
 * The 3D path only sets sampler types. The 2D fallback path also swaps texture lookup helpers to dimension-specific
 * implementations while preserving all unrelated substitutions.
 */
std::unordered_map<std::string, std::string> shaderReplacementsForTextureDimension(
  const std::unordered_map<std::string, std::string>& replacements,
  TextureDimension dimension,
  const TextureLookupReplacementSources& lookupSources);

} // namespace rendering
