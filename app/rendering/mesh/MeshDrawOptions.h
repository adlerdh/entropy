#pragma once

#include <glm/vec4.hpp>

#include <vector>

namespace rendering::mesh
{

inline constexpr int MaxMeshClipPlanes = 8; //!< Maximum active clip planes uploaded to the basic mesh shader

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
