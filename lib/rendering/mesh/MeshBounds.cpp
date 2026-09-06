#include "rendering/mesh/MeshBounds.h"

#include <glm/common.hpp>
#include <glm/vec4.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <ranges>

namespace rendering::mesh
{

bool isFinite(const glm::vec3& value) noexcept
{
  return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

bool isFinite(const glm::vec4& value) noexcept
{
  return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z) && std::isfinite(value.w);
}

std::optional<MeshBounds> computeBounds(std::span<const glm::vec3> positions)
{
  auto firstFinite = std::ranges::find_if(positions, [](const glm::vec3& position) { return isFinite(position); });
  if (firstFinite == positions.end()) {
    return std::nullopt;
  }

  MeshBounds bounds{*firstFinite, *firstFinite};
  for (const glm::vec3& position : positions) {
    if (!isFinite(position)) {
      continue;
    }

    bounds.min = glm::min(bounds.min, position);
    bounds.max = glm::max(bounds.max, position);
  }

  return bounds;
}

std::optional<MeshBounds> computeBounds(const MeshData& mesh)
{
  return computeBounds(mesh.positions);
}

MeshBounds transformedBounds(const MeshBounds& bounds, const glm::mat4& world_T_mesh) noexcept
{
  const std::array corners{
    glm::vec3{bounds.min.x, bounds.min.y, bounds.min.z},
    glm::vec3{bounds.max.x, bounds.min.y, bounds.min.z},
    glm::vec3{bounds.min.x, bounds.max.y, bounds.min.z},
    glm::vec3{bounds.max.x, bounds.max.y, bounds.min.z},
    glm::vec3{bounds.min.x, bounds.min.y, bounds.max.z},
    glm::vec3{bounds.max.x, bounds.min.y, bounds.max.z},
    glm::vec3{bounds.min.x, bounds.max.y, bounds.max.z},
    glm::vec3{bounds.max.x, bounds.max.y, bounds.max.z}};

  MeshBounds worldBounds{
    glm::vec3{world_T_mesh * glm::vec4{corners.front(), 1.0f}},
    glm::vec3{world_T_mesh * glm::vec4{corners.front(), 1.0f}}};
  for (const glm::vec3& corner : corners) {
    const glm::vec3 worldCorner = glm::vec3{world_T_mesh * glm::vec4{corner, 1.0f}};
    worldBounds.min = glm::min(worldBounds.min, worldCorner);
    worldBounds.max = glm::max(worldBounds.max, worldCorner);
  }

  return worldBounds;
}

std::optional<MeshBounds> computeWorldBounds(const MeshRenderable& renderable, const MeshData& mesh)
{
  const std::optional<MeshBounds> meshBounds = computeBounds(mesh);
  if (!meshBounds) {
    return std::nullopt;
  }

  return transformedBounds(*meshBounds, renderable.world_T_mesh);
}

std::optional<MeshBounds> computeWorldBounds(
  std::span<const std::reference_wrapper<const MeshRenderable>> renderables,
  const std::function<const MeshData*(const MeshHandle&)>& meshLookup)
{
  std::optional<MeshBounds> combined;
  for (const std::reference_wrapper<const MeshRenderable> renderableRef : renderables) {
    const MeshRenderable& renderable = renderableRef.get();
    const MeshData* mesh = meshLookup(renderable.mesh);
    if (!mesh) {
      continue;
    }

    const std::optional<MeshBounds> bounds = computeWorldBounds(renderable, *mesh);
    if (!bounds) {
      continue;
    }

    if (!combined) {
      combined = bounds;
      continue;
    }

    combined->min = glm::min(combined->min, bounds->min);
    combined->max = glm::max(combined->max, bounds->max);
  }

  return combined;
}

std::optional<MeshBounds> computeWorldBounds(
  const MeshRenderList& list,
  const std::function<const MeshData*(const MeshHandle&)>& meshLookup)
{
  auto mergeBounds = [](std::optional<MeshBounds>& combined, const std::optional<MeshBounds>& bounds) {
    if (!bounds) {
      return;
    }

    if (!combined) {
      combined = bounds;
      return;
    }

    combined->min = glm::min(combined->min, bounds->min);
    combined->max = glm::max(combined->max, bounds->max);
  };

  std::optional<MeshBounds> combined = computeWorldBounds(list.opaque, meshLookup);
  mergeBounds(combined, computeWorldBounds(list.alphaOverDdp, meshLookup));
  mergeBounds(combined, computeWorldBounds(list.additive, meshLookup));
  mergeBounds(combined, computeWorldBounds(list.multiplicative, meshLookup));

  return combined;
}

glm::vec3 center(const MeshBounds& bounds) noexcept
{
  return (bounds.min + bounds.max) * 0.5f;
}

glm::vec3 diagonal(const MeshBounds& bounds) noexcept
{
  return bounds.max - bounds.min;
}

} // namespace rendering::mesh
