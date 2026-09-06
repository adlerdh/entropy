#pragma once

#include "common/IntersectionTypes.h"
#include "rendering/mesh/MeshData.h"

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include <array>
#include <cstdint>
#include <optional>
#include <vector>

namespace rendering::mesh
{

/**
 * @brief Orthogonal world plane that can be shown as a textured image slice in a 3D view.
 */
enum class MeshImagePlaneOrientation
{
  Axial,   //!< Plane normal along world superior/inferior
  Coronal, //!< Plane normal along world posterior/anterior
  Sagittal //!< Plane normal along world left/right
};

/**
 * @brief Inputs used to clip an orthogonal 3D image plane against one image domain.
 */
struct MeshImagePlaneSceneInputs
{
  glm::mat4 world_T_crosshairs{1.0f};                       //!< Crosshairs frame transform in world/LPS coordinates
  glm::mat4 world_T_pixel{1.0f};                            //!< Transform from image pixel to world coordinates
  glm::mat4 pixel_T_world{1.0f};                            //!< Transform from world coordinates to image pixel
  glm::mat4 texture_T_world{1.0f};                          //!< Transform from world to normalized image texture
  std::array<glm::vec3, 8> pixelBoxCorners{};               //!< Image pixel-domain box corners
  std::vector<MeshImagePlaneOrientation> orientations = {}; //!< Plane orientations to generate
  float borderWidthWorld = 0.0f;                            //!< Optional border width in world units
};

/**
 * @brief One clipped and textured image-plane mesh.
 */
struct MeshImagePlaneSceneMesh
{
  MeshImagePlaneOrientation orientation = MeshImagePlaneOrientation::Axial; //!< Plane orientation
  MeshData mesh;                                                            //!< World-space textured mesh
  std::optional<MeshData> borderMesh;                                       //!< Optional boundary mesh
};

/**
 * @brief World-space normal for an orthogonal image plane orientation
 * @param orientation Plane orientation
 * @return Unit normal in world/LPS coordinates
 */
glm::vec3 imagePlaneWorldNormal(
  MeshImagePlaneOrientation orientation,
  const glm::mat4& world_T_crosshairs = glm::mat4{1.0f}) noexcept;

/**
 * @brief Opacity multiplier used to fade image planes by view angle
 * @param planeNormalWorld Unit or non-unit world-space plane normal
 * @param viewDirectionWorld Unit or non-unit world-space view direction
 * @return Clamped opacity multiplier in `[0, 1]`
 */
float imagePlaneViewOpacityMultiplier(const glm::vec3& planeNormalWorld, const glm::vec3& viewDirectionWorld) noexcept;

/**
 * @brief Version key for cached image-plane geometry.
 *
 * The plane vertices and texture coordinates are baked in world space, so every input that affects either must be
 * represented in the cache version. In particular, this includes the image's manual and loaded affine transforms.
 */
std::uint64_t imagePlaneSceneGeometryVersion(
  const MeshImagePlaneSceneInputs& inputs,
  MeshImagePlaneOrientation orientation) noexcept;

/** @brief Version key for cached world-space image-volume bounds geometry. */
std::uint64_t imageBoxSceneGeometryVersion(
  const std::array<glm::vec3, 8>& worldCorners,
  float borderWidthWorld) noexcept;

/**
 * @brief Build clipped world-space meshes for requested orthogonal image planes.
 *
 * Intersections are computed in image pixel space so off-axis images are clipped against the real image slab. The
 * resulting polygon is transformed back to world space before texture coordinates are assigned.
 *
 * @param inputs Image geometry, crosshairs position, and requested orientations
 * @return One mesh per requested plane that intersects the image domain
 */
std::vector<MeshImagePlaneSceneMesh> buildOrthogonalImagePlaneSceneMeshes(const MeshImagePlaneSceneInputs& inputs);

} // namespace rendering::mesh
