#include "rendering/mesh/MeshCrosshairsPolicy.h"

#include <algorithm>

namespace rendering::mesh
{

bool shouldRenderMeshCrosshairsGlyph(const MeshCrosshairsGlyphInputs& inputs) noexcept
{
  return inputs.showCrosshairsIn3D && !inputs.cameraFollowsCrosshairs && inputs.color.a > 0.0f &&
         inputs.diameterVoxelDiagonals > 0.0f && inputs.voxelDiagonalWorld > 0.0f;
}

MeshSphereGlyphStyle meshCrosshairsSphereGlyphStyle(const MeshCrosshairsGlyphInputs& inputs) noexcept
{
  return MeshSphereGlyphStyle{
    .radiusWorld = 0.5f * std::max(inputs.diameterVoxelDiagonals, 0.0f) * std::max(inputs.voxelDiagonalWorld, 0.0f),
    .color = inputs.color,
    .compositingMode = MeshCompositingMode::Opaque,
    .visible = shouldRenderMeshCrosshairsGlyph(inputs)};
}

} // namespace rendering::mesh
