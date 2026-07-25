#pragma once

#include "common/IntersectionTypes.h"
#include "rendering/mesh/MeshData.h"

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/mat4x4.hpp>

#include <optional>

namespace rendering::mesh
{

/**
 * @brief World-space geometry request for one textured image plane
 */
struct MeshImagePlane
{
  glm::vec3 centerWorld = glm::vec3{0.0f};                 //!< Plane center in world coordinates
  glm::vec3 uDirectionWorld = glm::vec3{1.0f, 0.0f, 0.0f}; //!< Horizontal plane direction
  glm::vec3 vDirectionWorld = glm::vec3{0.0f, 1.0f, 0.0f}; //!< Vertical plane direction
  glm::vec2 sizeWorld = glm::vec2{1.0f};                   //!< Plane size along u and v in world units
};

/**
 * @brief Create an axial LPS image plane with patient left on the right side of the view
 * @param centerWorld Plane center in world coordinates
 * @param sizeWorld Plane size along view-right and view-up axes
 * @return Axial plane request
 */
MeshImagePlane makeAxialImagePlane(const glm::vec3& centerWorld, const glm::vec2& sizeWorld) noexcept;

/**
 * @brief Create a coronal LPS image plane with superior up
 * @param centerWorld Plane center in world coordinates
 * @param sizeWorld Plane size along view-right and view-up axes
 * @return Coronal plane request
 */
MeshImagePlane makeCoronalImagePlane(const glm::vec3& centerWorld, const glm::vec2& sizeWorld) noexcept;

/**
 * @brief Create a sagittal LPS image plane with superior up
 * @param centerWorld Plane center in world coordinates
 * @param sizeWorld Plane size along view-right and view-up axes
 * @return Sagittal plane request
 */
MeshImagePlane makeSagittalImagePlane(const glm::vec3& centerWorld, const glm::vec2& sizeWorld) noexcept;

/**
 * @brief Create a two-triangle mesh for one 3D image plane
 * @param plane Plane center, axes, and size in world coordinates
 * @return World-space plane mesh, or empty when the plane is degenerate
 */
std::optional<MeshData> makeImagePlaneMesh(const MeshImagePlane& plane);

/**
 * @brief Add image texture coordinates to a world-space mesh
 * @param mesh World-space mesh whose positions should be sampled in image texture coordinates
 * @param texture_T_world Transform from world/LPS coordinates to normalized image texture coordinates
 * @return Mesh with per-vertex texture coordinates, or empty when the transform produces invalid coordinates
 */
std::optional<MeshData> withImageTextureCoordinates(const MeshData& mesh, const glm::mat4& texture_T_world);

/**
 * @brief Create a two-triangle textured mesh for one 3D image plane
 * @param plane Plane center, axes, and size in world coordinates
 * @param texture_T_world Transform from world/LPS coordinates to normalized image texture coordinates
 * @return World-space plane mesh with image texture coordinates, or empty when the plane or transform is degenerate
 */
std::optional<MeshData> makeTexturedImagePlaneMesh(const MeshImagePlane& plane, const glm::mat4& texture_T_world);

/**
 * @brief Create a triangle-fan mesh from a clipped image slice intersection
 * @param intersections Six polygon vertices plus centroid from the slice intersector
 * @return World-space triangle mesh, or empty when fewer than three unique vertices remain
 */
std::optional<MeshData> makeImageSliceIntersectionMesh(const intersection::IntersectionVertices& intersections);

/**
 * @brief Create a triangle-fan mesh from homogeneous clipped image slice intersections
 * @param intersections Six homogeneous polygon vertices plus centroid from the slice intersector
 * @return World-space triangle mesh, or empty when homogeneous conversion or polygon construction fails
 */
std::optional<MeshData> makeImageSliceIntersectionMesh(const intersection::IntersectionVerticesVec4& intersections);

/**
 * @brief Create a textured triangle-fan mesh from clipped image slice intersections
 * @param intersections Six polygon vertices plus centroid from the slice intersector
 * @param texture_T_world Transform from world/LPS coordinates to normalized image texture coordinates
 * @return World-space triangle mesh with image texture coordinates, or empty when geometry or texture mapping fails
 */
std::optional<MeshData> makeTexturedImageSliceIntersectionMesh(
  const intersection::IntersectionVertices& intersections,
  const glm::mat4& texture_T_world);

/**
 * @brief Create a textured triangle-fan mesh from homogeneous clipped image slice intersections
 * @param intersections Six homogeneous polygon vertices plus centroid from the slice intersector
 * @param texture_T_world Transform from world/LPS coordinates to normalized image texture coordinates
 * @return World-space triangle mesh with image texture coordinates, or empty when geometry or texture mapping fails
 */
std::optional<MeshData> makeTexturedImageSliceIntersectionMesh(
  const intersection::IntersectionVerticesVec4& intersections,
  const glm::mat4& texture_T_world);

} // namespace rendering::mesh
