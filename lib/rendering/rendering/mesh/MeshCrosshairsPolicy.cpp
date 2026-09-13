#include "rendering/mesh/MeshCrosshairsPolicy.h"

#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>

namespace rendering::mesh
{

bool shouldRenderMeshCrosshairsGlyph(const MeshCrosshairsGlyphInputs& inputs) noexcept
{
  return inputs.showCrosshairsIn3D && !inputs.cameraFollowsCrosshairs && inputs.diameterScenePercent > 0.0f &&
         inputs.lengthScenePercent > 0.0f && inputs.sceneDiagonalWorld > 0.0f;
}

MeshCrosshairsGlyphStyle meshCrosshairsGlyphStyle(const MeshCrosshairsGlyphInputs& inputs) noexcept
{
  constexpr float percentToFraction = 0.01f;
  return MeshCrosshairsGlyphStyle{
    .radiusWorld = 0.5f * percentToFraction * std::max(inputs.diameterScenePercent, 0.0f) *
                   std::max(inputs.sceneDiagonalWorld, 0.0f),
    .halfLengthWorld =
      0.5f * percentToFraction * std::max(inputs.lengthScenePercent, 0.0f) * std::max(inputs.sceneDiagonalWorld, 0.0f),
    .visible = shouldRenderMeshCrosshairsGlyph(inputs)};
}

std::array<glm::mat4, 3> meshCrosshairsAxisWorldTransforms(
  const glm::mat4& world_T_crosshairs,
  const MeshCrosshairsGlyphStyle& style) noexcept
{
  const std::array rotations{
    glm::rotate(glm::mat4{1.0f}, glm::half_pi<float>(), glm::vec3{0.0f, 1.0f, 0.0f}),
    glm::rotate(glm::mat4{1.0f}, -glm::half_pi<float>(), glm::vec3{1.0f, 0.0f, 0.0f}),
    glm::mat4{1.0f}};
  const glm::mat4 scale =
    glm::scale(glm::mat4{1.0f}, glm::vec3{style.radiusWorld, style.radiusWorld, style.halfLengthWorld});

  std::array<glm::mat4, 3> transforms{};
  for (std::size_t axis = 0; axis < transforms.size(); ++axis) {
    transforms[axis] = world_T_crosshairs * rotations[axis] * scale;
  }
  return transforms;
}

} // namespace rendering::mesh
