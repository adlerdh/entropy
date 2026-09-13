#pragma once

#include "rendering/mesh/MeshData.h"
#include "rendering/mesh/MeshRenderList.h"
#include "rendering/mesh/MeshRenderable.h"

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include <functional>
#include <optional>
#include <span>

namespace rendering::mesh
{

/**
 * @brief Axis-aligned bounding box for mesh-space or world-space positions
 */
struct MeshBounds
{
  glm::vec3 min = glm::vec3{0.0f}; //!< Minimum corner
  glm::vec3 max = glm::vec3{0.0f}; //!< Maximum corner
};

/**
 * @brief Return true when all vector components are finite
 * @param value Vector to inspect
 * @return Whether the vector contains only finite values
 */
bool isFinite(const glm::vec3& value) noexcept;

/**
 * @brief Return true when all vector components are finite
 * @param value Vector to inspect
 * @return Whether the vector contains only finite values
 */
bool isFinite(const glm::vec4& value) noexcept;

/**
 * @brief Compute a finite-position bounding box
 * @param positions Vertex positions
 * @return Bounding box, or empty when no finite position exists
 */
std::optional<MeshBounds> computeBounds(std::span<const glm::vec3> positions);

/**
 * @brief Compute a finite-position bounding box for a mesh
 * @param mesh Mesh whose positions define the bounds
 * @return Bounding box, or empty when no finite position exists
 */
std::optional<MeshBounds> computeBounds(const MeshData& mesh);

/**
 * @brief Compute world-space bounds after applying a mesh transform
 * @param bounds Mesh-space or source-space bounds
 * @param world_T_mesh Transform from mesh coordinates to world coordinates
 * @return World-space axis-aligned bounds
 */
MeshBounds transformedBounds(const MeshBounds& bounds, const glm::mat4& world_T_mesh) noexcept;

/**
 * @brief Compute world-space bounds for a renderable and its CPU mesh data
 * @param renderable Renderable containing the mesh-to-world transform
 * @param mesh CPU mesh data referenced by the renderable
 * @return World-space bounds, or empty when no finite mesh position exists
 */
std::optional<MeshBounds> computeWorldBounds(const MeshRenderable& renderable, const MeshData& mesh);

/**
 * @brief Compute combined world-space bounds for visible render-list entries
 * @param renderables Renderable references to inspect
 * @param meshLookup Callback that resolves a renderable handle to CPU mesh data
 * @return Combined bounds, or empty when no referenced mesh has finite positions
 * @throw Propagates allocation failures from the lookup callback
 */
std::optional<MeshBounds> computeWorldBounds(
  std::span<const std::reference_wrapper<const MeshRenderable>> renderables,
  const std::function<const MeshData*(const MeshHandle&)>& meshLookup);

/**
 * @brief Compute combined world-space bounds for every render-list bucket
 * @param list Render list to inspect
 * @param meshLookup Callback that resolves a renderable handle to CPU mesh data
 * @return Combined bounds, or empty when no referenced mesh has finite positions
 * @throw Propagates allocation failures from the lookup callback
 */
std::optional<MeshBounds> computeWorldBounds(
  const MeshRenderList& list,
  const std::function<const MeshData*(const MeshHandle&)>& meshLookup);

/**
 * @brief Return the center of a bounding box
 * @param bounds Bounding box
 * @return Center point
 */
glm::vec3 center(const MeshBounds& bounds) noexcept;

/**
 * @brief Return the diagonal vector of a bounding box
 * @param bounds Bounding box
 * @return `bounds.max - bounds.min`
 */
glm::vec3 diagonal(const MeshBounds& bounds) noexcept;

} // namespace rendering::mesh
