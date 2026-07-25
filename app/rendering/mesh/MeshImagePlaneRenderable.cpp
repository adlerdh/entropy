#include "rendering/mesh/MeshImagePlaneRenderable.h"

namespace rendering::mesh
{

MeshImagePlaneRenderable makeImagePlaneRenderable(
  const MeshHandle mesh,
  const glm::mat4& world_T_mesh,
  const MeshImagePlaneTexture texture,
  const bool visible)
{
  return MeshImagePlaneRenderable{.mesh = mesh, .world_T_mesh = world_T_mesh, .texture = texture, .visible = visible};
}

bool isDrawableImagePlaneRenderable(const MeshImagePlaneRenderable& renderable) noexcept
{
  return renderable.visible && !renderable.mesh.uid.is_nil() && !renderable.texture.imageUid.is_nil();
}

} // namespace rendering::mesh
