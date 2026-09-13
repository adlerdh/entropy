#pragma once

#include "rendering/mesh/MeshImagePlaneRenderable.h"

#include <functional>
#include <span>
#include <vector>

namespace rendering::mesh
{

/**
 * @brief Filtered draw list for textured image-plane renderables
 *
 * Image planes are drawn by an image-sampling shader path rather than the material mesh shader, so they intentionally
 * have their own render list instead of entering the ordinary mesh compositing buckets.
 */
struct MeshImagePlaneRenderList
{
  std::vector<std::reference_wrapper<const MeshImagePlaneRenderable>> imagePlanes; //!< Drawable image planes
};

/**
 * @brief Filter textured image-plane renderables into a draw list
 * @param imagePlanes Input image-plane renderables in deterministic scene order
 * @return Draw list containing only drawable image planes
 * @throw Propagates allocation failures
 */
MeshImagePlaneRenderList buildImagePlaneRenderList(std::span<const MeshImagePlaneRenderable> imagePlanes);

/**
 * Return one orientation's planes without changing their image-layer order.
 *
 * The preserved order is used to alpha-compose image 0, then image 1, and so on before the resulting plane enters DDP.
 */
MeshImagePlaneRenderList imagePlaneRenderListForOrientation(
  const MeshImagePlaneRenderList& list,
  MeshImagePlaneOrientation orientation);

/** Return the number of orthogonal orientations that contain at least one drawable image plane. */
std::size_t visibleImagePlaneOrientationCount(const MeshImagePlaneRenderList& list) noexcept;

/**
 * @brief Return the number of drawable image planes in a render list
 * @param list Image-plane render list
 * @return Drawable image-plane count
 */
std::size_t visibleImagePlaneCount(const MeshImagePlaneRenderList& list) noexcept;

} // namespace rendering::mesh
