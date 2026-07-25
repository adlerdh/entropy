#include "rendering/mesh/MeshImagePlaneRenderList.h"

namespace rendering::mesh
{

MeshImagePlaneRenderList buildImagePlaneRenderList(std::span<const MeshImagePlaneRenderable> imagePlanes)
{
  MeshImagePlaneRenderList list;

  for (const MeshImagePlaneRenderable& imagePlane : imagePlanes) {
    if (isDrawableImagePlaneRenderable(imagePlane)) {
      list.imagePlanes.emplace_back(imagePlane);
    }
  }

  return list;
}

std::size_t visibleImagePlaneCount(const MeshImagePlaneRenderList& list) noexcept
{
  return list.imagePlanes.size();
}

} // namespace rendering::mesh
