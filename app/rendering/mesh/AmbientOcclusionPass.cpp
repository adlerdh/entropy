#include "rendering/mesh/AmbientOcclusionPass.h"

#include "rendering/mesh/AmbientOcclusionResources.h"
#include "rendering/utility/gl/GLFrameBufferObject.h"
#include "rendering/utility/gl/GLShaderProgram.h"
#include "rendering/utility/gl/GLTexture.h"
#include "rendering/utility/gl/GLVertexArrayObject.h"
#include "rendering/utility/gl/OpenGLStateGuard.h"

#include <glad/glad.h>
#include <glm/vec2.hpp>

#include <array>
#include <cstdint>

namespace rendering::mesh
{

namespace
{

constexpr uint32_t k_normalTextureUnit = 0u;
constexpr uint32_t k_depthTextureUnit = 1u;
constexpr uint32_t k_rawOcclusionTextureUnit = 2u;

struct GlViewport
{
  GLint x = 0;
  GLint y = 0;
  GLsizei width = 0;
  GLsizei height = 0;
};

GlViewport currentViewport() noexcept
{
  std::array<GLint, 4> values{};
  glGetIntegerv(GL_VIEWPORT, values.data());
  return GlViewport{.x = values[0], .y = values[1], .width = values[2], .height = values[3]};
}

glm::uvec2 viewportSize(const GlViewport& viewport) noexcept
{
  return glm::uvec2{
    viewport.width > 0 ? static_cast<uint32_t>(viewport.width) : 0u,
    viewport.height > 0 ? static_cast<uint32_t>(viewport.height) : 0u};
}

void drawFullScreenTriangle(MeshAmbientOcclusionResources& resources)
{
  resources.fullScreenVao().bind();
  resources.fullScreenVao().drawArrays(PrimitiveMode::Triangles, 0, 3);
  resources.fullScreenVao().unbind();
}

void renderGeometry(const MeshAmbientOcclusionRenderRequest& request, const glm::uvec2& size)
{
  request.resources.geometryFbo().bind(fbo::TargetType::DrawAndRead);
  glViewport(0, 0, static_cast<GLsizei>(size.x), static_cast<GLsizei>(size.y));
  glDisable(GL_SCISSOR_TEST);
  glDrawBuffer(GL_COLOR_ATTACHMENT0);
  glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
  glDisable(GL_BLEND);
  glEnable(GL_DEPTH_TEST);
  glDepthMask(GL_TRUE);
  glEnable(GL_CULL_FACE);
  glCullFace(GL_BACK);
  glClearColor(0.5f, 0.5f, 1.0f, 0.0f);
  glClearDepth(1.0);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  request.meshRenderer.drawBucket(request.renderables, request.context, request.geometryProgram);
}

void resolveOcclusion(const MeshAmbientOcclusionRenderRequest& request, const glm::uvec2& size)
{
  request.resources.occlusionFbo().bind(fbo::TargetType::DrawAndRead);
  glViewport(0, 0, static_cast<GLsizei>(size.x), static_cast<GLsizei>(size.y));
  glDisable(GL_SCISSOR_TEST);
  glDrawBuffer(GL_COLOR_ATTACHMENT0);
  glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
  glDisable(GL_DEPTH_TEST);
  glDepthMask(GL_FALSE);
  glDisable(GL_BLEND);
  glClearColor(1.0f, 0.0f, 0.0f, 0.0f);
  glClear(GL_COLOR_BUFFER_BIT);

  request.resources.normalTexture().bind(k_normalTextureUnit);
  request.resources.depthTexture().bind(k_depthTextureUnit);
  request.resolveProgram.use();
  request.resolveProgram.setUniform("u_normalTex", static_cast<GLint>(k_normalTextureUnit));
  request.resolveProgram.setUniform("u_depthTex", static_cast<GLint>(k_depthTextureUnit));
  request.resolveProgram.setUniform("u_viewportSize", glm::vec2{size});
  request.resolveProgram.setUniform("u_camera_T_clip", request.context.camera_T_clip);
  request.resolveProgram.setUniform("u_clip_T_camera", request.context.clip_T_camera);
  request.resolveProgram.setUniform("u_camera_T_worldNormal", glm::mat3{request.context.camera_T_world});
  request.resolveProgram.setUniform("u_radiusMm", request.plan.radiusMm);
  request.resolveProgram.setUniform("u_strength", request.plan.strength);
  request.resolveProgram.setUniform("u_sampleCount", static_cast<GLint>(request.plan.sampleCount));
  drawFullScreenTriangle(request.resources);
  request.resolveProgram.stopUse();
  request.resources.depthTexture().unbind(k_depthTextureUnit);
  request.resources.normalTexture().unbind(k_normalTextureUnit);
}

void filterOcclusion(const MeshAmbientOcclusionRenderRequest& request, const glm::uvec2& size)
{
  request.resources.filteredOcclusionFbo().bind(fbo::TargetType::DrawAndRead);
  glViewport(0, 0, static_cast<GLsizei>(size.x), static_cast<GLsizei>(size.y));
  glDisable(GL_SCISSOR_TEST);
  glDrawBuffer(GL_COLOR_ATTACHMENT0);
  glDisable(GL_DEPTH_TEST);
  glDepthMask(GL_FALSE);
  glDisable(GL_BLEND);

  request.resources.normalTexture().bind(k_normalTextureUnit);
  request.resources.depthTexture().bind(k_depthTextureUnit);
  request.resources.rawOcclusionTexture().bind(k_rawOcclusionTextureUnit);
  request.filterProgram.use();
  request.filterProgram.setUniform("u_normalTex", static_cast<GLint>(k_normalTextureUnit));
  request.filterProgram.setUniform("u_depthTex", static_cast<GLint>(k_depthTextureUnit));
  request.filterProgram.setUniform("u_occlusionTex", static_cast<GLint>(k_rawOcclusionTextureUnit));
  request.filterProgram.setUniform("u_viewportSize", glm::vec2{size});
  request.filterProgram.setUniform("u_camera_T_clip", request.context.camera_T_clip);
  request.filterProgram.setUniform("u_radiusMm", request.plan.radiusMm);
  request.filterProgram.setUniform("u_power", request.plan.power);
  request.filterProgram.setUniform("u_contrast", request.plan.contrast);
  drawFullScreenTriangle(request.resources);
  request.filterProgram.stopUse();
  request.resources.rawOcclusionTexture().unbind(k_rawOcclusionTextureUnit);
  request.resources.depthTexture().unbind(k_depthTextureUnit);
  request.resources.normalTexture().unbind(k_normalTextureUnit);
}

} // namespace

bool renderMeshAmbientOcclusion(const MeshAmbientOcclusionRenderRequest& request)
{
  if (request.plan.state != MeshAdvancedLightingFeatureState::Enabled || request.renderables.empty()) {
    return false;
  }

  const OpenGLStateGuard scopedState{
    {k_normalTextureUnit, GL_TEXTURE_2D},
    {k_depthTextureUnit, GL_TEXTURE_2D},
    {k_rawOcclusionTextureUnit, GL_TEXTURE_2D}};
  const glm::uvec2 size = viewportSize(currentViewport());
  if (!request.resources.ensureSize(size) && !request.resources.initialized()) {
    return false;
  }

  renderGeometry(request, size);
  resolveOcclusion(request, size);
  filterOcclusion(request, size);
  return true;
}

} // namespace rendering::mesh
