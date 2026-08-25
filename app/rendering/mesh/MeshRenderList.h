#pragma once

#include "rendering/mesh/MeshRenderable.h"

#include <functional>
#include <span>
#include <vector>

namespace rendering::mesh
{

/**
 * @brief Draw buckets prepared from a mesh scene for one frame
 *
 * Renderables are stored as references so the render list can be rebuilt cheaply each frame without copying material
 * and transform state. The source renderables must outlive the render list.
 */
struct MeshRenderList
{
  std::vector<std::reference_wrapper<const MeshRenderable>> opaque;         //!< Opaque renderables
  std::vector<std::reference_wrapper<const MeshRenderable>> alphaOverDdp;   //!< DDP alpha-over renderables
  std::vector<std::reference_wrapper<const MeshRenderable>> additive;       //!< Additive renderables
  std::vector<std::reference_wrapper<const MeshRenderable>> multiplicative; //!< Multiplicative renderables
};

/**
 * @brief Partition visible renderables into compositing buckets
 * @param renderables Input renderables in deterministic scene order
 * @return Render list with invisible renderables removed
 * @throw Propagates allocation failures
 */
MeshRenderList buildRenderList(std::span<const MeshRenderable> renderables);

/**
 * @brief Return true when a render list requires dual depth peeling
 * @param list Render list to inspect
 * @return Whether any visible renderable requests alpha-over DDP
 */
bool requiresDdp(const MeshRenderList& list) noexcept;

/**
 * @brief Return the number of visible renderables in all buckets
 * @param list Render list to inspect
 * @return Total visible renderable count
 */
std::size_t visibleRenderableCount(const MeshRenderList& list) noexcept;

/** @brief Return opaque and alpha-over renderables that contribute to the mesh shadow map. */
std::vector<std::reference_wrapper<const MeshRenderable>> shadowCastingRenderables(const MeshRenderList& list);

} // namespace rendering::mesh
