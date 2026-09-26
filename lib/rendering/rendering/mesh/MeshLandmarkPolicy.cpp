#include "rendering/mesh/MeshLandmarkPolicy.h"

#include "mesh/MeshCompositing.h"
#include "mesh/MeshGlyphs.h"

#include <glm/vec4.hpp>
#include <algorithm>

namespace rendering::mesh
{

bool shouldRenderMeshLandmarkGlyph(const MeshLandmarkGlyphInputs& inputs) noexcept
{
  return inputs.groupVisible && inputs.pointVisible && inputs.groupOpacity > 0.0f && inputs.radiusScenePercent > 0.0f &&
         inputs.sceneDiagonalWorld > 0.0f;
}

MeshSphereGlyphStyle meshLandmarkSphereGlyphStyle(const MeshLandmarkGlyphInputs& inputs) noexcept
{
  const glm::vec3 rgb = inputs.groupColorOverride ? inputs.groupColor : inputs.pointColor;
  return MeshSphereGlyphStyle{
    .radiusWorld = std::max(inputs.radiusScenePercent, 0.0f) * 0.01f * std::max(inputs.sceneDiagonalWorld, 0.0f),
    .color = glm::vec4{rgb, std::clamp(inputs.groupOpacity, 0.0f, 1.0f)},
    .compositingMode = MeshCompositingMode::AlphaOverDdp,
    .visible = shouldRenderMeshLandmarkGlyph(inputs)};
}

} // namespace rendering::mesh
