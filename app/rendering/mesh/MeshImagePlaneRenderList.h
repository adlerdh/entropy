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
 * @brief Return the number of drawable image planes in a render list
 * @param list Image-plane render list
 * @return Drawable image-plane count
 */
std::size_t visibleImagePlaneCount(const MeshImagePlaneRenderList& list) noexcept;

} // namespace rendering::mesh
