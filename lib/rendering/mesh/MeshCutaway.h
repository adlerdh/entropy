#pragma once

#include "rendering/mesh/MeshDrawOptions.h"

#include <glm/mat3x3.hpp>
#include <glm/vec3.hpp>

#include <optional>

namespace rendering::mesh
{

/**
 * @brief Build an octant cutaway whose removed half-spaces point toward the viewer
 * @param originWorld Common world-space origin of the three cutting planes
 * @param worldAxes Columns containing the three cutaway axes in world coordinates
 * @param viewerWorldPosition Viewer position in world coordinates
 * @return Enabled normalized cutaway, or empty when its inputs do not define three valid axes
 */
std::optional<MeshOctantCutaway> viewerFacingOctantCutaway(
  const glm::vec3& originWorld,
  const glm::mat3& worldAxes,
  const glm::vec3& viewerWorldPosition) noexcept;

/** Return a normalized cutaway, or empty when an enabled cutaway contains an invalid plane. */
std::optional<MeshOctantCutaway> normalizedOctantCutaway(const MeshOctantCutaway& cutaway) noexcept;

/** Return whether a world position lies in the octant removed by an enabled cutaway. */
bool pointInsideRemovedOctant(const glm::vec3& worldPosition, const MeshOctantCutaway& cutaway) noexcept;

} // namespace rendering::mesh
