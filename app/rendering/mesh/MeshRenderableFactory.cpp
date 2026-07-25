#include "rendering/mesh/MeshRenderableFactory.h"

namespace rendering::mesh
{

namespace
{

MeshRenderable makeRenderable(
  const MeshHandle mesh,
  const glm::mat4& world_T_mesh,
  const MeshMaterial& material,
  const MeshCompositingMode compositingMode,
  const MeshFillMode fillMode,
  const bool backfaceCulling,
  const bool visible)
{
  MeshRenderable renderable;
  renderable.mesh = mesh;
  renderable.world_T_mesh = world_T_mesh;
  renderable.material = material;
  renderable.compositingMode = compositingMode;
  renderable.drawOptions.fillMode = fillMode;
  renderable.drawOptions.pickingMode = MeshPickingMode::Triangle;
  renderable.drawOptions.backfaceCulling = backfaceCulling;
  renderable.visible = visible && material.baseColor.a > 0.0f;
  return renderable;
}

} // namespace

MeshRenderable
makeIsosurfaceRenderable(const MeshHandle mesh, const glm::mat4& world_T_mesh, const IsosurfaceMeshStyle& style)
{
  return makeRenderable(
    mesh,
    world_T_mesh,
    style.material,
    style.compositingMode,
    style.fillMode,
    style.backfaceCulling,
    style.visible);
}

MeshRenderable makeSegmentationLabelRenderable(
  const MeshHandle mesh,
  const glm::mat4& world_T_mesh,
  const SegmentationLabelMeshStyle& style)
{
  return makeRenderable(
    mesh,
    world_T_mesh,
    style.material,
    style.compositingMode,
    style.fillMode,
    style.backfaceCulling,
    style.visible);
}

} // namespace rendering::mesh
