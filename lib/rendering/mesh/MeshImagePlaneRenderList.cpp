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

MeshImagePlaneRenderList imagePlaneRenderListForOrientation(
  const MeshImagePlaneRenderList& list,
  const MeshImagePlaneOrientation orientation)
{
  MeshImagePlaneRenderList filtered;
  filtered.imagePlanes.reserve(list.imagePlanes.size());
  for (const std::reference_wrapper<const MeshImagePlaneRenderable> renderable : list.imagePlanes) {
    if (renderable.get().orientation == orientation) {
      filtered.imagePlanes.push_back(renderable);
    }
  }
  return filtered;
}

std::size_t visibleImagePlaneOrientationCount(const MeshImagePlaneRenderList& list) noexcept
{
  bool axial = false;
  bool coronal = false;
  bool sagittal = false;
  for (const std::reference_wrapper<const MeshImagePlaneRenderable> renderable : list.imagePlanes) {
    switch (renderable.get().orientation) {
      case MeshImagePlaneOrientation::Axial:
        axial = true;
        break;
      case MeshImagePlaneOrientation::Coronal:
        coronal = true;
        break;
      case MeshImagePlaneOrientation::Sagittal:
        sagittal = true;
        break;
    }
  }
  return static_cast<std::size_t>(axial) + static_cast<std::size_t>(coronal) + static_cast<std::size_t>(sagittal);
}

std::size_t visibleImagePlaneCount(const MeshImagePlaneRenderList& list) noexcept
{
  return list.imagePlanes.size();
}

} // namespace rendering::mesh
