#pragma once

#include "rendering/mesh/MeshBounds.h"
#include "rendering/mesh/MeshDrawOptions.h"

#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <cstddef>
#include <optional>
#include <span>
#include <vector>

namespace rendering::mesh
{

/**
 * @brief Relationship between a box and the inside half-space of a clipping plane
 */
enum class ClipPlaneBoxClassification
{
  Outside,
  Intersecting,
  Inside
};

/**
 * @brief Normalize a world-space clipping plane equation
 * @param worldPlane Plane equation with any non-zero normal length
 * @return Normalized plane, or empty when the normal is invalid
 */
std::optional<glm::vec4> normalizedClipPlane(const glm::vec4& worldPlane) noexcept;

/**
 * @brief Signed distance from a point to a normalized clipping plane
 * @param normalizedWorldPlane Plane equation with unit-length normal
 * @param worldPosition Point in world coordinates
 * @return Signed distance where positive means inside
 */
float signedDistanceToPlane(const glm::vec4& normalizedWorldPlane, const glm::vec3& worldPosition) noexcept;

/**
 * @brief Return true when a point is inside all enabled clipping planes
 * @param worldPosition Point in world coordinates
 * @param clipPlanes Clipping planes to evaluate
 * @return Whether the point is in every enabled inside half-space
 */
bool pointInsideEnabledClipPlanes(const glm::vec3& worldPosition, std::span<const MeshClipPlane> clipPlanes) noexcept;

/**
 * @brief Return enabled, normalized clipping planes suitable for shader upload
 * @param clipPlanes Input clipping planes in world coordinates
 * @param maxPlaneCount Maximum number of valid planes to return
 * @return Enabled normalized planes, with invalid and excess planes omitted
 */
std::vector<glm::vec4> enabledNormalizedClipPlanes(
  std::span<const MeshClipPlane> clipPlanes,
  std::size_t maxPlaneCount = static_cast<std::size_t>(MaxMeshClipPlanes));

/**
 * @brief Classify an axis-aligned bounding box against one clipping plane
 * @param bounds World-space bounds
 * @param clipPlane Clipping plane
 * @return Whether the box is outside, intersecting, or inside
 */
ClipPlaneBoxClassification classifyBoundsAgainstClipPlane(
  const MeshBounds& bounds,
  const MeshClipPlane& clipPlane) noexcept;

} // namespace rendering::mesh
