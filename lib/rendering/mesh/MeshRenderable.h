#pragma once

#include "rendering/mesh/MeshCompositing.h"
#include "rendering/mesh/MeshDrawOptions.h"
#include "rendering/mesh/MeshHandle.h"
#include "rendering/mesh/MeshMaterial.h"

#include <glm/mat4x4.hpp>

namespace rendering::mesh
{

/**
 * @brief View-specific description of how one mesh should be drawn
 *
 * A renderable references mesh geometry by handle and stores only transform, material, compositing, draw, and
 * visibility state. It does not own CPU mesh data or GPU buffers.
 */
struct MeshRenderable
{
  MeshHandle mesh;                          //!< Referenced mesh geometry
  glm::mat4 world_T_mesh = glm::mat4{1.0f}; //!< Transform from mesh coordinates to world coordinates
  MeshMaterial material;                    //!< Surface appearance
  MeshCompositingMode compositingMode = MeshCompositingMode::Opaque; //!< Compositing path
  MeshDrawOptions drawOptions;                                       //!< Non-geometric draw behavior
  bool visible = true;     //!< Whether this renderable participates in draw lists
  bool castsShadow = true; //!< Whether this renderable contributes to the mesh shadow map
};

} // namespace rendering::mesh
