#pragma once

#include "common/IntersectionTypes.h"
#include "rendering/mesh/MeshData.h"

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include <array>
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
  glm::vec3 worldCrosshairs{0.0f};                          //!< Plane origin in world/LPS coordinates
  glm::mat4 world_T_pixel{1.0f};                            //!< Transform from image pixel to world coordinates
  glm::mat4 pixel_T_world{1.0f};                            //!< Transform from world coordinates to image pixel
  glm::mat4 texture_T_world{1.0f};                          //!< Transform from world to normalized image texture
  std::array<glm::vec3, 8> pixelBoxCorners{};               //!< Image pixel-domain box corners
  std::vector<MeshImagePlaneOrientation> orientations = {}; //!< Plane orientations to generate
};

/**
 * @brief One clipped and textured image-plane mesh.
 */
struct MeshImagePlaneSceneMesh
{
  MeshImagePlaneOrientation orientation = MeshImagePlaneOrientation::Axial; //!< Plane orientation
  MeshData mesh;                                                            //!< World-space textured mesh
};

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
