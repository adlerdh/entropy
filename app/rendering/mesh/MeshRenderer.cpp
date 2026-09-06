#include "rendering/mesh/MeshRenderer.h"

#include "rendering/mesh/MeshClipPlanes.h"
#include "rendering/mesh/MeshMaterial.h"
#include "rendering/utility/gl/GLShaderProgram.h"
#include "rendering/utility/gl/GLTexture.h"
#include "rendering/utility/gl/OpenGLStateGuard.h"

#include <glad/glad.h>

#include <glm/gtc/matrix_inverse.hpp>

#include <cstddef>
#include <string>
#include <vector>

namespace rendering::mesh
{

namespace
{

constexpr uint32_t k_shadowMapTextureUnit = 7u;
constexpr uint32_t k_ambientOcclusionTextureUnit = 6u;

GLenum polygonModeForFillMode(const MeshFillMode fillMode) noexcept
{
  switch (fillMode) {
    case MeshFillMode::Surface:
    case MeshFillMode::SurfaceWithWireframe:
      return GL_FILL;
    case MeshFillMode::Wireframe:
      return GL_LINE;
    case MeshFillMode::Points:
      return GL_POINT;
  }

  return GL_FILL;
}

// Visible mesh passes run after 2D image and NanoVG work, so they must not inherit stencil/depth state
void applyRasterState(const MeshDrawOptions& drawOptions, const glm::mat4& world_T_mesh)
{
  glPolygonMode(GL_FRONT_AND_BACK, polygonModeForFillMode(drawOptions.fillMode));
  if (drawOptions.backfaceCulling) {
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(meshTransformReversesOrientation(world_T_mesh) ? GL_CW : GL_CCW);
  }
  else {
    glDisable(GL_CULL_FACE);
  }
}

void applyShadowDepthRasterState()
{
  // Shadow casters may be open surfaces, and their visible style may be wireframe or points. A complete, two-sided
  // filled depth map is required regardless of those presentation settings.
  glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
  glDisable(GL_CULL_FACE);
}

void uploadClipPlanes(const MeshDrawOptions& drawOptions, GLShaderProgram& program)
{
  const std::vector<glm::vec4> clipPlanes = enabledNormalizedClipPlanes(drawOptions.clipPlanes);
  program.setUniform("u_clipPlaneCount", static_cast<GLint>(clipPlanes.size()));
  for (std::size_t i = 0; i < clipPlanes.size(); ++i) {
    program.setUniform("u_clipPlanes[" + std::to_string(i) + "]", clipPlanes[i]);
  }
}

void drawUploadedMesh(const MeshGpuData& gpuData)
{
  gpuData.vao().bind();
  gpuData.vao().drawElements(gpuData.drawParams());
  gpuData.vao().unbind();
}

int shaderValue(const MeshShadingModel shadingModel) noexcept
{
  switch (shadingModel) {
    case MeshShadingModel::Unlit:
      return 0;
    case MeshShadingModel::SimpleLit:
      return 1;
    case MeshShadingModel::PhysicallyBased:
      return 2;
  }

  return 0;
}

bool shadowsActive(const MeshDrawContext& context) noexcept
{
  return context.advancedLighting.shadows.state == MeshAdvancedLightingFeatureState::Enabled &&
         context.shadowDepthTexture != nullptr;
}

bool ambientOcclusionActive(const MeshDrawContext& context) noexcept
{
  return context.advancedLighting.ambientOcclusion.state == MeshAdvancedLightingFeatureState::Enabled &&
         context.ambientOcclusionTexture != nullptr;
}

void uploadShadowUniforms(const MeshDrawContext& context, GLShaderProgram& program)
{
  const bool enabled = shadowsActive(context);
  program.setUniform("u_shadowMapEnabled", enabled);
  program.setUniform("u_lightClip_T_world", context.shadowLightClip_T_world);
  program.setUniform("u_shadowStrength", context.advancedLighting.shadows.strength);
  program.setUniform("u_shadowDepthBias", context.advancedLighting.shadows.depthBias);
  program.setUniform("u_lightDirectionWorld", context.lightDirectionWorld);
  program.setSamplerUniform("u_shadowMapTex", static_cast<GLint>(k_shadowMapTextureUnit));
  if (enabled) {
    context.shadowDepthTexture->bind(k_shadowMapTextureUnit);
  }
}

void uploadAmbientOcclusionUniforms(const MeshDrawContext& context, GLShaderProgram& program)
{
  const bool enabled = ambientOcclusionActive(context);
  program.setUniform("u_screenAmbientOcclusionEnabled", false);
  program.setSamplerUniform("u_screenAmbientOcclusionTex", static_cast<GLint>(k_ambientOcclusionTextureUnit));
  program.setUniform("u_viewportOrigin", context.viewportOrigin);
  if (enabled) {
    context.ambientOcclusionTexture->bind(k_ambientOcclusionTextureUnit);
  }
}

void releaseShadowTexture(const MeshDrawContext& context)
{
  if (shadowsActive(context)) {
    context.shadowDepthTexture->unbind(k_shadowMapTextureUnit);
  }
}

void releaseAmbientOcclusionTexture(const MeshDrawContext& context)
{
  if (ambientOcclusionActive(context)) {
    context.ambientOcclusionTexture->unbind(k_ambientOcclusionTextureUnit);
  }
}

} // namespace

void MeshRenderer::drawOpaque(const MeshRenderList& list, const MeshDrawContext& context, GLShaderProgram& program)
{
  const OpenGLStateGuard state{{k_shadowMapTextureUnit, GL_TEXTURE_2D}, {k_ambientOcclusionTextureUnit, GL_TEXTURE_2D}};
  glDisable(GL_BLEND);
  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_LESS);
  glDepthMask(GL_TRUE);
  glDisable(GL_STENCIL_TEST);
  drawBucket(list.opaque, context, program);
}

void MeshRenderer::drawAdditive(const MeshRenderList& list, const MeshDrawContext& context, GLShaderProgram& program)
{
  const OpenGLStateGuard state{{k_shadowMapTextureUnit, GL_TEXTURE_2D}, {k_ambientOcclusionTextureUnit, GL_TEXTURE_2D}};
  glEnable(GL_BLEND);
  glBlendEquation(GL_FUNC_ADD);
  glBlendFuncSeparate(GL_ONE, GL_ONE, GL_ONE, GL_ONE);
  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_LESS);
  glDepthMask(GL_FALSE);
  glDisable(GL_STENCIL_TEST);
  drawBucket(list.additive, context, program);
}

void MeshRenderer::drawMultiplicative(
  const MeshRenderList& list,
  const MeshDrawContext& context,
  GLShaderProgram& program)
{
  const OpenGLStateGuard state{{k_shadowMapTextureUnit, GL_TEXTURE_2D}, {k_ambientOcclusionTextureUnit, GL_TEXTURE_2D}};
  glEnable(GL_BLEND);
  glBlendEquation(GL_FUNC_ADD);
  glBlendFuncSeparate(GL_DST_COLOR, GL_ZERO, GL_ONE, GL_ZERO);
  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_LESS);
  glDepthMask(GL_FALSE);
  glDisable(GL_STENCIL_TEST);
  drawBucket(list.multiplicative, context, program);
}

void MeshRenderer::drawImplementedBuckets(
  const MeshRenderList& list,
  const MeshDrawContext& context,
  GLShaderProgram& program)
{
  drawOpaque(list, context, program);
  drawAdditive(list, context, program);
  drawMultiplicative(list, context, program);
}

void MeshRenderer::drawBucket(
  std::span<const std::reference_wrapper<const MeshRenderable>> renderables,
  const MeshDrawContext& context,
  GLShaderProgram& program,
  const MeshDrawPass pass)
{
  if (!context.meshLookup) {
    return;
  }

  const OpenGLStateGuard state{{k_shadowMapTextureUnit, GL_TEXTURE_2D}, {k_ambientOcclusionTextureUnit, GL_TEXTURE_2D}};
  program.use();
  program.setUniform("u_clip_T_world", context.clip_T_world);
  if (pass == MeshDrawPass::Surface) {
    program.setUniform("u_cameraWorldPosition", context.cameraWorldPosition);
    program.setUniform("u_lightingAmbient", context.lighting.x);
    program.setUniform("u_lightingDiffuse", context.lighting.y);
    program.setUniform("u_lightingSpecular", context.lighting.z);
    program.setUniform("u_lightingSpecularPower", context.lighting.w);
    uploadShadowUniforms(context, program);
    uploadAmbientOcclusionUniforms(context, program);
  }

  for (const std::reference_wrapper<const MeshRenderable> renderableRef : renderables) {
    const MeshRenderable& renderable = renderableRef.get();
    const MeshGpuData* gpuData = context.meshLookup(renderable.mesh);
    if (!gpuData) {
      continue;
    }

    const glm::mat3 world_T_meshNormal = glm::inverseTranspose(glm::mat3{renderable.world_T_mesh});
    program.setUniform("u_world_T_mesh", renderable.world_T_mesh);
    program.setUniform("u_world_T_meshNormal", world_T_meshNormal);
    program.setUniform("u_hasVertexNormals", gpuData->hasNormals());
    if (pass == MeshDrawPass::Surface) {
      const MeshMaterial material = sanitizedMaterial(renderable.material, context.fallbackColor);
      program.setUniform(
        "u_screenAmbientOcclusionEnabled",
        ambientOcclusionActive(context) && renderable.compositingMode == MeshCompositingMode::Opaque);
      program.setUniform("u_baseColor", material.baseColor);
      program.setUniform("u_metallic", material.metallic);
      program.setUniform("u_roughness", material.roughness);
      program.setUniform("u_ambientOcclusion", material.ambientOcclusion);
      program.setUniform("u_shadingModel", shaderValue(material.shadingModel));
      program.setUniform("u_flatShadingEnabled", material.flatShadingEnabled);
      program.setUniform("u_triangleEdgesEnabled", material.triangleEdgesEnabled);
      program.setUniform("u_triangleEdgeColor", material.triangleEdgeColor);
      program.setUniform("u_rimLightingEnabled", material.rimLightingEnabled);
      program.setUniform("u_rimOpacityStrength", material.rimOpacityStrength);
      program.setUniform("u_rimEmissionStrength", material.rimEmissionStrength);
      program.setUniform("u_rimPower", material.rimPower);
      program.setUniform("u_hasVertexColors", gpuData->hasColors());
    }
    else if (pass == MeshDrawPass::AmbientOcclusionGeometry) {
      const MeshMaterial material = sanitizedMaterial(renderable.material, context.fallbackColor);
      program.setUniform("u_flatShadingEnabled", material.flatShadingEnabled);
    }
    uploadClipPlanes(renderable.drawOptions, program);

    pass == MeshDrawPass::ShadowDepth ? applyShadowDepthRasterState()
                                      : applyRasterState(renderable.drawOptions, renderable.world_T_mesh);
    drawUploadedMesh(*gpuData);

    if (pass == MeshDrawPass::Surface && renderable.drawOptions.fillMode == MeshFillMode::SurfaceWithWireframe) {
      glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
      drawUploadedMesh(*gpuData);
    }
  }

  program.stopUse();
  if (pass == MeshDrawPass::Surface) {
    releaseAmbientOcclusionTexture(context);
    releaseShadowTexture(context);
  }
}

} // namespace rendering::mesh
