#pragma once

#include "rendering/mesh/MeshAdvancedLighting.h"
#include "rendering/mesh/MeshRenderer.h"

#include <functional>
#include <span>

class GLShaderProgram;

namespace rendering::mesh
{

class MeshShadowMapResources;

/**
 * @brief Inputs needed to render one directional-light shadow map for mesh geometry
 */
struct MeshShadowMapRenderRequest
{
  MeshShadowMapResources& resources; //!< Shadow-map framebuffer and depth texture owner
  std::span<const std::reference_wrapper<const MeshRenderable>> renderables; //!< Meshes included in the shadow map
  const MeshDrawContext& context;   //!< Light-space matrix and mesh lookup state
  const MeshShadowPlan& plan;       //!< Sanitized shadow-map execution plan
  const MeshRenderer& meshRenderer; //!< Stateless mesh draw helper
  GLShaderProgram& depthProgram;    //!< Depth-only mesh shader program
};

/**
 * @brief Render mesh depth from light space into a shadow map
 *
 * The pass renders only when `request.plan.state` is `Enabled`. It restores framebuffer, viewport, color mask, depth,
 * and culling state before returning.
 *
 * @param request Resources, program, renderables, and light-space state for one shadow-map pass
 * @return True when a shadow map was rendered
 * @throw Propagates OpenGL allocation or shader errors
 */
bool renderMeshShadowMap(const MeshShadowMapRenderRequest& request);

} // namespace rendering::mesh
