#pragma once

#include "rendering/mesh/MeshDdpPolicy.h"
#include "rendering/mesh/MeshRenderList.h"
#include "rendering/mesh/MeshRenderer.h"

#include <functional>
#include <span>

class GLShaderProgram;
class GLTexture;

namespace rendering::mesh
{

class MeshDdpResources;

/**
 * @brief Inputs needed to execute one dual depth peeling pass for alpha-over mesh renderables
 */
struct MeshDdpRenderRequest
{
  MeshDdpResources& resources;                                               //!< DDP framebuffer and texture owner
  std::span<const std::reference_wrapper<const MeshRenderable>> renderables; //!< Meshes resolved by DDP
  const MeshDrawContext& context;             //!< Camera, lighting, and mesh lookup state
  const MeshDdpPlan& plan;                    //!< Sanitized DDP execution plan
  const MeshRenderer& meshRenderer;           //!< Stateless mesh draw helper
  GLShaderProgram& initProgram;               //!< Mesh fragment shader that initializes depth bounds
  GLShaderProgram& peelProgram;               //!< Mesh fragment shader that peels front/back layers
  GLShaderProgram& backBlendProgram;          //!< Full-screen shader that accumulates back colors
  GLShaderProgram& resolveProgram;            //!< Full-screen shader that resolves front over back
  std::function<void()> drawExtraDepthBounds; //!< Optional non-material renderables for the DDP initialization pass
  std::function<void(GLTexture&, GLTexture&)> drawExtraPeelLayers; //!< Optional non-material peel pass
};

/**
 * @brief Render alpha-over mesh renderables with dual depth peeling into the current viewport
 *
 * The current draw framebuffer and viewport are treated as the final render target. The pass temporarily redirects
 * drawing into its private DDP framebuffer, then restores the original framebuffer and viewport for the final resolve.
 *
 * @param request Resources, programs, render list, and camera state for one DDP pass
 * @throw Propagates OpenGL errors reported by the GL wrapper layer
 */
void renderMeshDdpAlphaOver(const MeshDdpRenderRequest& request);

} // namespace rendering::mesh
