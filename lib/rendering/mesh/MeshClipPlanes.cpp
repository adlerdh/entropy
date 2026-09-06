#include "rendering/mesh/MeshClipPlanes.h"

#include <glm/geometric.hpp>

#include <algorithm>
#include <array>
#include <cmath>

namespace rendering::mesh
{

std::optional<glm::vec4> normalizedClipPlane(const glm::vec4& worldPlane) noexcept
{
  const auto normal = glm::vec3{worldPlane};
  const float length = glm::length(normal);
  if (!std::isfinite(length) || length <= 0.0f) {
    return std::nullopt;
  }

  return worldPlane / length;
}

float signedDistanceToPlane(const glm::vec4& normalizedWorldPlane, const glm::vec3& worldPosition) noexcept
{
  return glm::dot(glm::vec3{normalizedWorldPlane}, worldPosition) + normalizedWorldPlane.w;
}

bool pointInsideEnabledClipPlanes(const glm::vec3& worldPosition, std::span<const MeshClipPlane> clipPlanes) noexcept
{
  return std::ranges::all_of(clipPlanes, [&worldPosition](const MeshClipPlane& clipPlane) {
    if (!clipPlane.enabled) {
      return true;
    }
    const std::optional<glm::vec4> normalizedPlane = normalizedClipPlane(clipPlane.worldPlane);
    return normalizedPlane && signedDistanceToPlane(*normalizedPlane, worldPosition) >= 0.0f;
  });
}

std::vector<glm::vec4> enabledNormalizedClipPlanes(
  std::span<const MeshClipPlane> clipPlanes,
  const std::size_t maxPlaneCount)
{
  std::vector<glm::vec4> normalizedPlanes;
  normalizedPlanes.reserve(std::min(clipPlanes.size(), maxPlaneCount));

  for (const MeshClipPlane& clipPlane : clipPlanes) {
    if (normalizedPlanes.size() >= maxPlaneCount) {
      break;
    }
    if (!clipPlane.enabled) {
      continue;
    }

    if (const std::optional<glm::vec4> normalizedPlane = normalizedClipPlane(clipPlane.worldPlane)) {
      normalizedPlanes.push_back(*normalizedPlane);
    }
  }

  return normalizedPlanes;
}

ClipPlaneBoxClassification classifyBoundsAgainstClipPlane(
  const MeshBounds& bounds,
  const MeshClipPlane& clipPlane) noexcept
{
  if (!clipPlane.enabled) {
    return ClipPlaneBoxClassification::Inside;
  }

  const std::optional<glm::vec4> normalizedPlane = normalizedClipPlane(clipPlane.worldPlane);
  if (!normalizedPlane) {
    return ClipPlaneBoxClassification::Inside;
  }

  const std::array corners{
    glm::vec3{bounds.min.x, bounds.min.y, bounds.min.z},
    glm::vec3{bounds.max.x, bounds.min.y, bounds.min.z},
    glm::vec3{bounds.min.x, bounds.max.y, bounds.min.z},
    glm::vec3{bounds.max.x, bounds.max.y, bounds.min.z},
    glm::vec3{bounds.min.x, bounds.min.y, bounds.max.z},
    glm::vec3{bounds.max.x, bounds.min.y, bounds.max.z},
    glm::vec3{bounds.min.x, bounds.max.y, bounds.max.z},
    glm::vec3{bounds.max.x, bounds.max.y, bounds.max.z}};

  bool hasInside = false;
  bool hasOutside = false;
  for (const glm::vec3& corner : corners) {
    const bool inside = signedDistanceToPlane(*normalizedPlane, corner) >= 0.0f;
    hasInside = hasInside || inside;
    hasOutside = hasOutside || !inside;
  }

  if (hasInside && hasOutside) {
    return ClipPlaneBoxClassification::Intersecting;
  }

  return hasInside ? ClipPlaneBoxClassification::Inside : ClipPlaneBoxClassification::Outside;
}

} // namespace rendering::mesh
