#pragma once

#include "rendering/mesh/MeshAdvancedLighting.h"
#include "rendering/mesh/MeshRenderer.h"

#include <functional>
#include <span>

class GLShaderProgram;

namespace rendering::mesh
{

class MeshAmbientOcclusionResources;

/**
 * @brief Inputs needed to render one screen-space mesh ambient occlusion texture
 */
struct MeshAmbientOcclusionRenderRequest
{
  MeshAmbientOcclusionResources& resources;                                  //!< AO framebuffer and texture owner
  std::span<const std::reference_wrapper<const MeshRenderable>> renderables; //!< Meshes included in the AO pre-pass
  const MeshDrawContext& context;       //!< Camera, matrices, and mesh lookup state
  const MeshAmbientOcclusionPlan& plan; //!< Sanitized AO execution plan
  const MeshRenderer& meshRenderer;     //!< Stateless mesh draw helper
  GLShaderProgram& geometryProgram;     //!< Mesh normal/depth geometry shader
  GLShaderProgram& resolveProgram;      //!< Full-screen AO resolve shader
};

/**
 * @brief Render a screen-space mesh ambient occlusion factor texture
 *
 * The pass renders only when `request.plan.state` is `Enabled`. It restores framebuffer, viewport, color mask, depth,
 * blending, and culling state before returning.
 *
 * @param request Resources, programs, renderables, and camera state for one AO pass
 * @return True when an AO texture was rendered
 * @throw Propagates OpenGL allocation or shader errors
 */
bool renderMeshAmbientOcclusion(const MeshAmbientOcclusionRenderRequest& request);

} // namespace rendering::mesh
