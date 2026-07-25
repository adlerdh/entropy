#include "rendering/mesh/MeshLandmarkPolicy.h"

#include <algorithm>

namespace rendering::mesh
{

bool shouldRenderMeshLandmarkGlyph(const MeshLandmarkGlyphInputs& inputs) noexcept
{
  return inputs.groupVisible && inputs.pointVisible && inputs.groupOpacity > 0.0f && inputs.radiusFactor > 0.0f &&
         inputs.voxelDiagonalWorld > 0.0f;
}

MeshSphereGlyphStyle meshLandmarkSphereGlyphStyle(const MeshLandmarkGlyphInputs& inputs) noexcept
{
  const glm::vec3 rgb = inputs.groupColorOverride ? inputs.groupColor : inputs.pointColor;
  return MeshSphereGlyphStyle{
    .radiusWorld = std::max(inputs.radiusFactor, 0.0f) * std::max(inputs.voxelDiagonalWorld, 0.0f),
    .color = glm::vec4{rgb, std::clamp(inputs.groupOpacity, 0.0f, 1.0f)},
    .compositingMode = MeshCompositingMode::AlphaOverDdp,
    .visible = shouldRenderMeshLandmarkGlyph(inputs)};
}

} // namespace rendering::mesh
