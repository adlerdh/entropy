#include "rendering/mesh/MeshGlyphs.h"

#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>

namespace rendering::mesh
{

namespace
{

MeshRenderable makeGlyphRenderable(
  const MeshHandle mesh,
  const glm::mat4& world_T_mesh,
  const glm::vec4& color,
  const MeshCompositingMode compositingMode,
  const bool visible)
{
  MeshRenderable renderable;
  renderable.mesh = mesh;
  renderable.world_T_mesh = world_T_mesh;
  renderable.material.baseColor = color;
  renderable.compositingMode = compositingMode;
  renderable.drawOptions.pickingMode = MeshPickingMode::Object;
  renderable.visible = visible && color.a > 0.0f;
  return renderable;
}

} // namespace

MeshRenderable
makeSphereGlyphRenderable(const MeshHandle sphereMesh, const glm::vec3& centerWorld, const MeshSphereGlyphStyle& style)
{
  const float radius = std::max(style.radiusWorld, 0.0f);
  const glm::mat4 world_T_mesh =
    glm::translate(glm::mat4{1.0f}, centerWorld) * glm::scale(glm::mat4{1.0f}, glm::vec3{radius});
  return makeGlyphRenderable(sphereMesh, world_T_mesh, style.color, style.compositingMode, style.visible);
}

MeshRenderable makeZAxisCylinderGlyphRenderable(
  const MeshHandle cylinderMesh,
  const glm::vec3& centerWorld,
  const MeshCylinderGlyphStyle& style)
{
  const float radius = std::max(style.radiusWorld, 0.0f);
  const float length = std::max(style.lengthWorld, 0.0f);
  const glm::mat4 world_T_mesh =
    glm::translate(glm::mat4{1.0f}, centerWorld) * glm::scale(glm::mat4{1.0f}, glm::vec3{radius, radius, length});
  return makeGlyphRenderable(cylinderMesh, world_T_mesh, style.color, style.compositingMode, style.visible);
}

} // namespace rendering::mesh
