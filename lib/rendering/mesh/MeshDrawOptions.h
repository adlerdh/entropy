#pragma once

#include <glm/mat3x3.hpp>
#include <glm/mat4x4.hpp>
#include <glm/matrix.hpp>
#include <glm/vec4.hpp>

#include <vector>

namespace rendering::mesh
{

inline constexpr int MaxMeshClipPlanes = 8; //!< Maximum active clip planes uploaded to the basic mesh shader

/** Return whether a mesh-to-world transform reverses triangle winding. */
inline bool meshTransformReversesOrientation(const glm::mat4& world_T_mesh) noexcept
{
  return glm::determinant(glm::mat3{world_T_mesh}) < 0.0f;
}

/**
 * @brief Rasterization style used to draw mesh geometry
 */
enum class MeshFillMode
{
  Surface,
  Wireframe,
  SurfaceWithWireframe,
  Points
};

/**
 * @brief Amount of mesh detail requested by picking
 */
enum class MeshPickingMode
{
  Disabled,
  Object,
  Triangle,
  Vertex
};

/**
 * @brief World-space clipping plane
 *
 * The inside half-space is `dot(worldPlane.xyz, worldPosition) + worldPlane.w >= 0`.
 */
struct MeshClipPlane
{
  glm::vec4 worldPlane = glm::vec4{0.0f, 0.0f, 1.0f, 0.0f}; //!< Normalized world-space plane equation
  bool enabled = true;                                      //!< Whether this clipping plane is active
};

/**
 * @brief Draw behavior that does not change mesh geometry
 */
struct MeshDrawOptions
{
  MeshFillMode fillMode = MeshFillMode::Surface;           //!< Surface, wireframe, overlay, or points
  MeshPickingMode pickingMode = MeshPickingMode::Disabled; //!< Picking detail for this renderable
  std::vector<MeshClipPlane> clipPlanes;                   //!< Enabled and disabled clipping planes
  bool backfaceCulling = false;                            //!< Whether back-facing triangles may be culled
};

} // namespace rendering::mesh
