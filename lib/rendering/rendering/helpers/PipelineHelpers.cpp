#include "rendering/helpers/PipelineHelpers.h"

#include "rendering/gl/GLShaderProgram.h"

#include <cstddef>
#include <limits>
#include <utility>

namespace rendering
{

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
  return layoutIt != std::end(textureLayouts) && TextureDimension::Texture3D == layoutIt->second.dimension;
}

bool imageHasMeshSceneTextureLayout(
  const std::unordered_map<uuids::uuid, PlanarTextureLayout>& textureLayouts,
  const uuids::uuid& imageUid)
{
  const auto layoutIt = textureLayouts.find(imageUid);
  return layoutIt != std::end(textureLayouts) && (TextureDimension::Texture2D == layoutIt->second.dimension ||
                                                  TextureDimension::Texture3D == layoutIt->second.dimension);
}

std::list<uuids::uuid> raycastableImageUids(
  const std::list<uuids::uuid>& imageUids,
  const std::unordered_map<uuids::uuid, PlanarTextureLayout>& textureLayouts)
{
  std::list<uuids::uuid> raycastableUids;
  std::copy_if(
    imageUids.begin(),
    imageUids.end(),
    std::back_inserter(raycastableUids),
    [&textureLayouts](const uuids::uuid& imageUid) {
      return imageHasRaycastableTextureLayout(textureLayouts, imageUid);
    });
  return raycastableUids;
}

GLShaderProgram& shaderProgramForTextureDimension(
  const std::unordered_map<ShaderProgramType, std::unique_ptr<GLShaderProgram>>& shaderPrograms3D,
  const std::unordered_map<ShaderProgramType, std::unique_ptr<GLShaderProgram>>& shaderPrograms2D,
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
