#pragma once

#include "rendering/mesh/MeshData.h"
#include "rendering/mesh/MeshDrawOptions.h"
#include "rendering/mesh/MeshRenderable.h"

#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include <cstdint>
#include <functional>
#include <optional>
#include <span>

namespace rendering::mesh
{

/**
 * @brief World-space ray used for mesh picking
 */
struct MeshPickRay
{
  glm::vec3 origin = glm::vec3{0.0f};                 //!< Ray origin
  glm::vec3 direction = glm::vec3{0.0f, 0.0f, -1.0f}; //!< Unit-length ray direction
};

/**
 * @brief Triangle hit returned by mesh picking helpers
 */
struct MeshTriangleHit
{
  float distance = 0.0f;         //!< Distance along the ray
  uint32_t triangleIndex = 0;    //!< Triangle index in the mesh index buffer
  glm::vec3 worldPosition{0.0f}; //!< Hit point in world coordinates
  glm::vec3 barycentric{0.0f};   //!< Barycentric coordinates `(w, u, v)`
};

/**
 * @brief Normalize a picking ray direction
 * @param ray Ray with any non-zero direction length
 * @return Ray with unit-length direction, or empty when the direction is invalid
 */
std::optional<MeshPickRay> normalizedPickRay(const MeshPickRay& ray) noexcept;

/**
 * @brief Intersect a ray with an axis-aligned bounding box
 * @param ray Ray with unit-length direction
 * @param minCorner Minimum box corner
 * @param maxCorner Maximum box corner
 * @return Nearest non-negative hit distance, or empty for no hit
 */
std::optional<float>
intersectRayAabb(const MeshPickRay& ray, const glm::vec3& minCorner, const glm::vec3& maxCorner) noexcept;

/**
 * @brief Intersect a ray with one triangle
 * @param ray Ray with unit-length direction
 * @param a First triangle vertex
 * @param b Second triangle vertex
 * @param c Third triangle vertex
 * @return Hit details, or empty for no hit
 */
std::optional<MeshTriangleHit>
intersectRayTriangle(const MeshPickRay& ray, const glm::vec3& a, const glm::vec3& b, const glm::vec3& c) noexcept;

/**
 * @brief Find the nearest picked triangle in a world-space mesh
 * @param mesh Mesh whose positions and indices are tested
 * @param ray Ray with unit-length direction
 * @param clipPlanes World-space clip planes to honor
 * @return Nearest hit, or empty for no hit
 */
std::optional<MeshTriangleHit>
pickNearestTriangle(const MeshData& mesh, const MeshPickRay& ray, std::span<const MeshClipPlane> clipPlanes = {});

/**
 * @brief Find the nearest picked triangle after transforming mesh vertices into world coordinates
 * @param mesh Mesh whose positions and indices are tested
 * @param ray World-space ray with unit-length direction
 * @param world_T_mesh Transform from mesh coordinates to world coordinates
 * @param clipPlanes World-space clip planes to honor
 * @return Nearest hit in world coordinates, or empty for no hit
 */
std::optional<MeshTriangleHit> pickNearestTriangle(
  const MeshData& mesh,
  const MeshPickRay& ray,
  const glm::mat4& world_T_mesh,
  std::span<const MeshClipPlane> clipPlanes = {});

/**
 * @brief Request used to pick the nearest mesh renderable in a view
 */
struct MeshScenePickRequest
{
  MeshPickRay worldRay;                                         //!< Pick ray in world coordinates
  std::span<const MeshRenderable> renderables;                  //!< Candidate renderables
  std::function<const MeshData*(const MeshHandle&)> meshLookup; //!< Resolve CPU mesh geometry by handle
};

/**
 * @brief Nearest scene-level mesh picking result
 */
struct MeshScenePickHit
{
  MeshHandle mesh;             //!< Picked mesh handle
  MeshTriangleHit triangleHit; //!< Picked triangle details in world coordinates
};

/**
 * @brief Pick the nearest visible renderable with picking enabled
 * @param request Pick ray, renderables, and CPU mesh lookup callback
 * @return Nearest picked renderable hit, or empty for no hit
 */
std::optional<MeshScenePickHit> pickNearestRenderable(const MeshScenePickRequest& request);

} // namespace rendering::mesh
