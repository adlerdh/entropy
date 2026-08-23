#include "rendering/mesh/AmbientOcclusionPass.h"

#include "rendering/mesh/AmbientOcclusionResources.h"
#include "rendering/utility/gl/GLFrameBufferObject.h"
#include "rendering/utility/gl/GLShaderProgram.h"
#include "rendering/utility/gl/GLTexture.h"
#include "rendering/utility/gl/GLVertexArrayObject.h"

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

class ScopedAoGlState
{
public:
  ScopedAoGlState()
  {
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &m_drawFramebuffer);
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &m_readFramebuffer);
    glGetIntegerv(GL_VIEWPORT, m_viewport.data());
    glGetIntegerv(GL_SCISSOR_BOX, m_scissor.data());
    glGetBooleanv(GL_COLOR_WRITEMASK, m_colorMask.data());
    glGetBooleanv(GL_DEPTH_WRITEMASK, &m_depthMask);
    glGetIntegerv(GL_BLEND_SRC_RGB, &m_blendSrcRgb);
    glGetIntegerv(GL_BLEND_DST_RGB, &m_blendDstRgb);
    glGetIntegerv(GL_BLEND_SRC_ALPHA, &m_blendSrcAlpha);
    glGetIntegerv(GL_BLEND_DST_ALPHA, &m_blendDstAlpha);
    glGetIntegerv(GL_BLEND_EQUATION_RGB, &m_blendEquationRgb);
    glGetIntegerv(GL_BLEND_EQUATION_ALPHA, &m_blendEquationAlpha);
    glGetIntegerv(GL_CULL_FACE_MODE, &m_cullFaceMode);
    glGetFloatv(GL_COLOR_CLEAR_VALUE, m_clearColor.data());
    glGetIntegerv(GL_ACTIVE_TEXTURE, &m_activeTexture);
    glGetIntegerv(GL_CURRENT_PROGRAM, &m_program);
    for (uint32_t unit = 0; unit < m_texture2DBindings.size(); ++unit) {
      glActiveTexture(GL_TEXTURE0 + unit);
      glGetIntegerv(GL_TEXTURE_BINDING_2D, &m_texture2DBindings[unit]);
    }
    glActiveTexture(static_cast<GLenum>(m_activeTexture));
    m_blendEnabled = glIsEnabled(GL_BLEND);
    m_scissorEnabled = glIsEnabled(GL_SCISSOR_TEST);
    m_depthTestEnabled = glIsEnabled(GL_DEPTH_TEST);
    m_cullFaceEnabled = glIsEnabled(GL_CULL_FACE);
  }

  ScopedAoGlState(const ScopedAoGlState&) = delete;
  ScopedAoGlState& operator=(const ScopedAoGlState&) = delete;

  ~ScopedAoGlState()
  {
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, static_cast<GLuint>(m_drawFramebuffer));
    glBindFramebuffer(GL_READ_FRAMEBUFFER, static_cast<GLuint>(m_readFramebuffer));
    glViewport(m_viewport[0], m_viewport[1], m_viewport[2], m_viewport[3]);
    glScissor(m_scissor[0], m_scissor[1], m_scissor[2], m_scissor[3]);
    glColorMask(m_colorMask[0], m_colorMask[1], m_colorMask[2], m_colorMask[3]);
    glDepthMask(m_depthMask);
    glBlendEquationSeparate(static_cast<GLenum>(m_blendEquationRgb), static_cast<GLenum>(m_blendEquationAlpha));
    glBlendFuncSeparate(
      static_cast<GLenum>(m_blendSrcRgb),
      static_cast<GLenum>(m_blendDstRgb),
      static_cast<GLenum>(m_blendSrcAlpha),
      static_cast<GLenum>(m_blendDstAlpha));
    m_blendEnabled ? glEnable(GL_BLEND) : glDisable(GL_BLEND);
    m_scissorEnabled ? glEnable(GL_SCISSOR_TEST) : glDisable(GL_SCISSOR_TEST);
    m_depthTestEnabled ? glEnable(GL_DEPTH_TEST) : glDisable(GL_DEPTH_TEST);
    m_cullFaceEnabled ? glEnable(GL_CULL_FACE) : glDisable(GL_CULL_FACE);
    glCullFace(static_cast<GLenum>(m_cullFaceMode));
    glClearColor(m_clearColor[0], m_clearColor[1], m_clearColor[2], m_clearColor[3]);
    for (uint32_t unit = 0; unit < m_texture2DBindings.size(); ++unit) {
      glActiveTexture(GL_TEXTURE0 + unit);
      glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(m_texture2DBindings[unit]));
    }
    glActiveTexture(static_cast<GLenum>(m_activeTexture));
    glUseProgram(static_cast<GLuint>(m_program));
  }

private:
  GLint m_drawFramebuffer = 0;
  GLint m_readFramebuffer = 0;
  std::array<GLint, 4> m_viewport{};
  std::array<GLint, 4> m_scissor{};
  std::array<GLboolean, 4> m_colorMask{};
  GLboolean m_depthMask = GL_TRUE;
  GLboolean m_blendEnabled = GL_FALSE;
  GLboolean m_scissorEnabled = GL_FALSE;
  GLboolean m_depthTestEnabled = GL_FALSE;
  GLboolean m_cullFaceEnabled = GL_FALSE;
  GLint m_blendSrcRgb = GL_ONE;
  GLint m_blendDstRgb = GL_ZERO;
  GLint m_blendSrcAlpha = GL_ONE;
  GLint m_blendDstAlpha = GL_ZERO;
  GLint m_blendEquationRgb = GL_FUNC_ADD;
  GLint m_blendEquationAlpha = GL_FUNC_ADD;
  GLint m_cullFaceMode = GL_BACK;
  std::array<GLfloat, 4> m_clearColor{};
  GLint m_activeTexture = GL_TEXTURE0;
  GLint m_program = 0;
  std::array<GLint, 3> m_texture2DBindings{};
};

void drawFullScreenTriangle(MeshAmbientOcclusionResources& resources)
{
  resources.fullScreenVao().bind();
  glDrawArrays(GL_TRIANGLES, 0, 3);
  resources.fullScreenVao().release();
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

  const ScopedAoGlState scopedState;
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
