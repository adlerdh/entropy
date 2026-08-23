#include "rendering/mesh/MeshCrosshairsPolicy.h"

#include <algorithm>

namespace rendering::mesh
{

bool shouldRenderMeshCrosshairsGlyph(const MeshCrosshairsGlyphInputs& inputs) noexcept
{
  return inputs.showCrosshairsIn3D && !inputs.cameraFollowsCrosshairs && inputs.diameterVoxelDiagonals > 0.0f &&
         inputs.lengthVoxelDiagonals > 0.0f && inputs.voxelDiagonalWorld > 0.0f;
}

MeshCrosshairsGlyphStyle meshCrosshairsGlyphStyle(const MeshCrosshairsGlyphInputs& inputs) noexcept
{
  return MeshCrosshairsGlyphStyle{
    .radiusWorld = 0.5f * std::max(inputs.diameterVoxelDiagonals, 0.0f) * std::max(inputs.voxelDiagonalWorld, 0.0f),
    .halfLengthWorld = 0.5f * std::max(inputs.lengthVoxelDiagonals, 0.0f) * std::max(inputs.voxelDiagonalWorld, 0.0f),
    .visible = shouldRenderMeshCrosshairsGlyph(inputs)};
}

} // namespace rendering::mesh
