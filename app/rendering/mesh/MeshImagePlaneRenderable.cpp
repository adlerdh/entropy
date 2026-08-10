#include "rendering/mesh/MeshImagePlaneRenderable.h"

namespace rendering::mesh
{

MeshImagePlaneRenderable makeImagePlaneRenderable(
  const MeshHandle mesh,
  const glm::mat4& world_T_mesh,
  const glm::vec3& centerWorld,
  const MeshImagePlaneTexture texture,
  const float opacityMultiplier,
  const bool shadingEnabled,
  const bool visible)
{
  return MeshImagePlaneRenderable{
    .mesh = mesh,
    .world_T_mesh = world_T_mesh,
    .centerWorld = centerWorld,
    .texture = texture,
    .opacityMultiplier = opacityMultiplier,
    .shadingEnabled = shadingEnabled,
    .visible = visible};
}

bool isDrawableImagePlaneRenderable(const MeshImagePlaneRenderable& renderable) noexcept
{
  return renderable.visible && renderable.opacityMultiplier > 0.0f && !renderable.mesh.uid.is_nil() &&
         !renderable.texture.imageUid.is_nil();
}

} // namespace rendering::mesh
