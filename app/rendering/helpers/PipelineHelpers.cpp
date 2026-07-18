#include "rendering/helpers/PipelineHelpers.h"

#include "rendering/utility/gl/GLShaderProgram.h"

#include <cstddef>
#include <iterator>
#include <limits>
#include <utility>

namespace rendering
{

std::string replacePlaceholders(
  const std::string& source,
  const std::unordered_map<std::string, std::string>& placeholdersToStringMap)
{
  std::string result = source;
  for (const auto& [placeholder, replacement] : placeholdersToStringMap) {
    std::size_t pos = 0;
    while ((pos = result.find(placeholder, pos)) != std::string::npos) {
      result.replace(pos, placeholder.length(), replacement);
      pos += replacement.length();
    }
  }
  return result;
}

std::unordered_map<std::string, std::string> shaderReplacementsForTextureDimension(
  const std::unordered_map<std::string, std::string>& replacements,
  const TextureDimension dimension,
  const TextureLookupReplacementSources& lookupSources)
{
  std::unordered_map<std::string, std::string> result = replacements;

  if (TextureDimension::Texture2D == dimension) {
    result["$$IMAGE_SAMPLER_TYPE$$"] = "sampler2D";
    result["$$SEG_SAMPLER_TYPE$$"] = "usampler2D";

    if (const auto it = result.find("$$TEXTURE_LOOKUP_FUNCTION$$"); it != std::end(result)) {
      if (it->second == lookupSources.cubic3D) {
        it->second = std::string{lookupSources.cubic2D};
      }
      else if (it->second == lookupSources.floatingPointLinear3D) {
        it->second = std::string{lookupSources.floatingPointLinear2D};
      }
      else {
        it->second = std::string{lookupSources.linear2D};
      }
    }

    if (const auto it = result.find("$$UINT_TEXTURE_LOOKUP_FUNCTION$$"); it != std::end(result)) {
      it->second = std::string{lookupSources.uintLinear2D};
    }

    return result;
  }

  result["$$IMAGE_SAMPLER_TYPE$$"] = "sampler3D";
  result["$$SEG_SAMPLER_TYPE$$"] = "usampler3D";
  return result;
}

uint32_t growBrushPreviewCapacity(const uint32_t current, const uint32_t required)
{
  if (current >= required) {
    return current;
  }

  if (0 == current) {
    return required;
  }

  const uint64_t grown = static_cast<uint64_t>(current) + std::max<uint64_t>(1, current / 2);
  return static_cast<uint32_t>(
    std::min<uint64_t>(std::max<uint64_t>(grown, required), std::numeric_limits<uint32_t>::max()));
}

glm::uvec3 growBrushPreviewCapacity(const glm::uvec3& current, const glm::uvec3& required)
{
  return glm::uvec3{
    growBrushPreviewCapacity(current.x, required.x),
    growBrushPreviewCapacity(current.y, required.y),
    growBrushPreviewCapacity(current.z, required.z)};
}

PlanarTextureLayout textureLayoutOrDefault(
  const std::unordered_map<uuids::uuid, PlanarTextureLayout>& layouts,
  const std::optional<uuids::uuid>& uid)
{
  if (!uid) {
    return {};
  }
  const auto it = layouts.find(*uid);
  return it != std::end(layouts) ? it->second : PlanarTextureLayout{};
}

bool isTexture2D(const PlanarTextureLayout& layout)
{
  return TextureDimension::Texture2D == layout.dimension;
}

glm::ivec2 textureAxesForProgramSlot(const PlanarTextureLayout& layout)
{
  return isTexture2D(layout) ? layout.axes : glm::ivec2{0, 1};
}

bool imageHasRaycastableTextureLayout(
  const std::unordered_map<uuids::uuid, PlanarTextureLayout>& textureLayouts,
  const uuids::uuid& imageUid)
{
  const auto layoutIt = textureLayouts.find(imageUid);
  return layoutIt == std::end(textureLayouts) || TextureDimension::Texture3D == layoutIt->second.dimension;
}

std::list<uuids::uuid> raycastableImageUids(
  const std::list<uuids::uuid>& imageUids,
  const std::unordered_map<uuids::uuid, PlanarTextureLayout>& textureLayouts)
{
  std::list<uuids::uuid> raycastableUids;
  for (const uuids::uuid& imageUid : imageUids) {
    if (imageHasRaycastableTextureLayout(textureLayouts, imageUid)) {
      raycastableUids.push_back(imageUid);
    }
  }
  return raycastableUids;
}

GLShaderProgram& shaderProgramForTextureDimension(
  std::unordered_map<ShaderProgramType, std::unique_ptr<GLShaderProgram>>& shaderPrograms3D,
  std::unordered_map<ShaderProgramType, std::unique_ptr<GLShaderProgram>>& shaderPrograms2D,
  const ShaderProgramType shaderType,
  const TextureDimension dimension)
{
  return *(TextureDimension::Texture2D == dimension ? shaderPrograms2D : shaderPrograms3D).at(shaderType);
}

void setTexture2DAxesUniforms(
  GLShaderProgram& program,
  const PlanarTextureLayout& slot0,
  const PlanarTextureLayout& slot1)
{
  program.setUniform("u_tex2DAxes[0]", textureAxesForProgramSlot(slot0));
  program.setUniform("u_tex2DAxes[1]", textureAxesForProgramSlot(slot1));
}

} // namespace rendering
