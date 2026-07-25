#pragma once

#include "rendering/mesh/MeshAdvancedLighting.h"
#include "rendering/mesh/MeshGpuData.h"
#include "rendering/mesh/MeshRenderList.h"

#include <glm/mat3x3.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <functional>
#include <span>

class GLShaderProgram;
class GLTexture;

namespace rendering::mesh
{

/**
 * @brief Camera and lookup state needed to draw a mesh render list
 */
struct MeshDrawContext
{
  glm::mat4 clip_T_world = glm::mat4{1.0f};                        //!< Transform from world to clip coordinates
  glm::vec3 cameraWorldPosition = glm::vec3{0.0f};                 //!< Camera eye position in world coordinates
  glm::vec3 lightDirectionWorld = glm::vec3{0.4f, 0.6f, 0.7f};     //!< Direction from the surface toward the light
  glm::vec4 fallbackColor = glm::vec4{0.8f, 0.8f, 0.8f, 1.0f};     //!< Color used when material color is invalid
  MeshAdvancedLightingPlan advancedLighting;                       //!< Sanitized optional lighting pass decisions
  glm::mat4 shadowLightClip_T_world = glm::mat4{1.0f};             //!< Transform from world to shadow-map clip space
  GLTexture* shadowDepthTexture = nullptr;                         //!< Shadow-map depth texture, if available
  GLTexture* ambientOcclusionTexture = nullptr;                    //!< Screen-space AO texture, if available
  std::function<const MeshGpuData*(const MeshHandle&)> meshLookup; //!< Resolve a mesh handle to uploaded buffers
};

/**
 * @brief Stateless draw helper for uploaded mesh geometry
 *
 * `MeshRenderer` draws already-uploaded meshes. It does not own GPU buffers, build render lists, inspect `AppData`, or
 * run mesh extraction.
 */
class MeshRenderer
{
public:
  /**
   * @brief Draw the opaque bucket of a render list
   * @param list Render list to draw
   * @param context Camera matrices and mesh lookup callback
   * @param program Linked mesh shader program
   */
  void drawOpaque(const MeshRenderList& list, const MeshDrawContext& context, GLShaderProgram& program) const;

  /**
   * @brief Draw the additive bucket of a render list using additive blending
   * @param list Render list to draw
   * @param context Camera matrices and mesh lookup callback
   * @param program Linked mesh shader program
   */
  void drawAdditive(const MeshRenderList& list, const MeshDrawContext& context, GLShaderProgram& program) const;

  /**
   * @brief Draw the multiplicative bucket of a render list using order-independent multiplicative blending
   * @param list Render list to draw
   * @param context Camera matrices and mesh lookup callback
   * @param program Linked mesh shader program
   */
  void drawMultiplicative(const MeshRenderList& list, const MeshDrawContext& context, GLShaderProgram& program) const;

  /**
   * @brief Draw all compositing buckets currently implemented by the mesh renderer
   *
   * This helper is intended for simple tests and non-DDP callers. Production 3D view rendering uses the higher-level
   * render-list draw helper so alpha-over DDP renderables can be resolved between opaque and additive passes.
   *
   * @param list Render list to draw
   * @param context Camera matrices and mesh lookup callback
   * @param program Linked mesh shader program
   */
  void drawImplementedBuckets(const MeshRenderList& list, const MeshDrawContext& context, GLShaderProgram& program)
    const;

  /**
   * @brief Draw one bucket of mesh renderables using the basic mesh shader
   * @param renderables Renderables to draw
   * @param context Camera matrices and mesh lookup callback
   * @param program Linked mesh shader program
   */
  void drawBucket(
    std::span<const std::reference_wrapper<const MeshRenderable>> renderables,
    const MeshDrawContext& context,
    GLShaderProgram& program) const;
};

} // namespace rendering::mesh
