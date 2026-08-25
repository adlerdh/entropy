#include "rendering/mesh/MeshRenderList.h"

namespace rendering::mesh
{

MeshRenderList buildRenderList(std::span<const MeshRenderable> renderables)
{
  MeshRenderList list;

  for (const MeshRenderable& renderable : renderables) {
    if (!renderable.visible) {
      continue;
    }

    switch (renderable.compositingMode) {
      case MeshCompositingMode::Opaque:
        list.opaque.emplace_back(renderable);
        break;
      case MeshCompositingMode::AlphaOverDdp:
        list.alphaOverDdp.emplace_back(renderable);
        break;
      case MeshCompositingMode::Additive:
        list.additive.emplace_back(renderable);
        break;
      case MeshCompositingMode::Multiplicative:
        list.multiplicative.emplace_back(renderable);
        break;
    }
  }

  return list;
}

bool requiresDdp(const MeshRenderList& list) noexcept
{
  return !list.alphaOverDdp.empty();
}

std::size_t visibleRenderableCount(const MeshRenderList& list) noexcept
{
  return list.opaque.size() + list.alphaOverDdp.size() + list.additive.size() + list.multiplicative.size();
}

std::vector<std::reference_wrapper<const MeshRenderable>> shadowCastingRenderables(const MeshRenderList& list)
{
  std::vector<std::reference_wrapper<const MeshRenderable>> renderables;
  renderables.reserve(list.opaque.size() + list.alphaOverDdp.size());
  const auto appendCasters = [&renderables](const auto& bucket) {
    for (const std::reference_wrapper<const MeshRenderable> renderable : bucket) {
      if (renderable.get().castsShadow) {
        renderables.push_back(renderable);
      }
    }
  };
  appendCasters(list.opaque);
  appendCasters(list.alphaOverDdp);
  return renderables;
}

} // namespace rendering::mesh
