#pragma once

#include <glm/glm.hpp>

#include <cstddef>
#include <functional>
#include <optional>
#include <span>

namespace camera3d
{

/**
 * @brief Description of a raycast isosurface hit.
 */
struct IsoSurfacePickHit
{
  glm::vec3 worldPosition{0.0f}; //!< Hit location in world/LPS coordinates
  float rayDistance{0.0f};       //!< Distance along the world-space ray
  std::size_t isoIndex{0};       //!< Index of the isovalue that was hit
};

/**
 * @brief Inputs for CPU raycast isosurface picking.
 *
 * The picker mirrors the volume raycaster's threshold-crossing rule, but keeps the implementation
 * independent from OpenGL so it can be unit-tested and used synchronously from mouse callbacks.
 */
struct IsoSurfacePickRequest
{
  glm::vec3 worldRayOrigin{0.0f};    //!< Ray origin in world/LPS coordinates
  glm::vec3 worldRayDirection{0.0f}; //!< Normalized ray direction in world/LPS coordinates
  glm::mat4 pixel_T_world{1.0f};     //!< Transform from world/LPS to continuous image voxel coordinates
  glm::mat4 world_T_pixel{1.0f};     //!< Transform from continuous image voxel coordinates to world/LPS
  glm::vec3 pixelDimensions{0.0f};   //!< Image dimensions in voxels
  float stepLength{1.0f};            //!< Positive ray step length in world/LPS units
  bool renderFrontFaces{true};       //!< Detect low-to-high threshold crossings
  bool renderBackFaces{true};        //!< Detect high-to-low threshold crossings

  std::span<const double> isoValues; //!< Native image-intensity isovalues to test
  std::function<std::optional<double>(const glm::vec3&)> sampleValue;
};

/**
 * @brief Pick the first ray/isovalue threshold crossing.
 * @return Closest hit, or std::nullopt if the ray misses the image or no isosurface is crossed.
 */
std::optional<IsoSurfacePickHit> pickFirstIsoSurfaceHit(const IsoSurfacePickRequest& request);

} // namespace camera3d
