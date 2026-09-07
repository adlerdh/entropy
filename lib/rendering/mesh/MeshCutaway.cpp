#include "rendering/mesh/MeshCutaway.h"

#include "rendering/mesh/MeshClipPlanes.h"

#include <glm/geometric.hpp>

#include <cstddef>
#include <cmath>

namespace rendering::mesh
{

std::optional<MeshOctantCutaway> normalizedOctantCutaway(const MeshOctantCutaway& cutaway) noexcept
{
  if (!cutaway.enabled) {
    return MeshOctantCutaway{};
  }

  MeshOctantCutaway normalized;
  normalized.enabled = true;
  for (std::size_t index = 0; index < normalized.worldPlanes.size(); ++index) {
    const std::optional<glm::vec4> plane = normalizedClipPlane(cutaway.worldPlanes[index]);
    if (!plane) {
      return std::nullopt;
    }
    normalized.worldPlanes[index] = *plane;
  }
  return normalized;
}

std::optional<MeshOctantCutaway> viewerFacingOctantCutaway(
  const glm::vec3& originWorld,
  const glm::mat3& worldAxes,
  const glm::vec3& viewerWorldPosition) noexcept
{
  if (
    !std::isfinite(originWorld.x) || !std::isfinite(originWorld.y) || !std::isfinite(originWorld.z) ||
    !std::isfinite(viewerWorldPosition.x) || !std::isfinite(viewerWorldPosition.y) ||
    !std::isfinite(viewerWorldPosition.z))
  {
    return std::nullopt;
  }

  const glm::vec3 originToViewer = viewerWorldPosition - originWorld;
  MeshOctantCutaway cutaway;
  cutaway.enabled = true;
  for (std::size_t index = 0; index < cutaway.worldPlanes.size(); ++index) {
    const glm::vec3 axis = worldAxes[index];
    const float axisLength = glm::length(axis);
    if (!std::isfinite(axisLength) || axisLength <= 0.0f) {
      return std::nullopt;
    }

    const glm::vec3 normalizedAxis = axis / axisLength;
    const float viewerSide = glm::dot(originToViewer, normalizedAxis) >= 0.0f ? 1.0f : -1.0f;
    const glm::vec3 viewerFacingNormal = viewerSide * normalizedAxis;
    cutaway.worldPlanes[index] = glm::vec4{viewerFacingNormal, -glm::dot(viewerFacingNormal, originWorld)};
  }
  return cutaway;
}

bool pointInsideRemovedOctant(const glm::vec3& worldPosition, const MeshOctantCutaway& cutaway) noexcept
{
  const std::optional<MeshOctantCutaway> normalized = normalizedOctantCutaway(cutaway);
  if (!normalized || !normalized->enabled) {
    return false;
  }

  for (const glm::vec4& plane : normalized->worldPlanes) {
    if (signedDistanceToPlane(plane, worldPosition) < 0.0f) {
      return false;
    }
  }
  return true;
}

} // namespace rendering::mesh
