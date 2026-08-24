#include "rendering/mesh/MeshDdpPass.h"

#include "rendering/mesh/MeshDdpResources.h"
#include "rendering/utility/gl/GLFrameBufferObject.h"
#include "rendering/utility/gl/GLShaderProgram.h"
#include "rendering/utility/gl/GLTexture.h"
#include "rendering/utility/gl/GLVertexArrayObject.h"

#include <glad/glad.h>

#include <glm/vec2.hpp>

#include <array>
#include <cstdint>
#include <optional>
#include <vector>

namespace rendering::mesh
{

namespace
{

constexpr std::array<GLenum, 7> k_peelAttachments{
  GL_COLOR_ATTACHMENT0,
  GL_COLOR_ATTACHMENT1,
  GL_COLOR_ATTACHMENT2,
  GL_COLOR_ATTACHMENT3,
  GL_COLOR_ATTACHMENT4,
  GL_COLOR_ATTACHMENT5,
  GL_COLOR_ATTACHMENT6};
constexpr uint32_t k_depthTextureUnit = 0u;
constexpr uint32_t k_frontColorTextureUnit = 1u;
constexpr uint32_t k_backTempTextureUnit = 0u;
constexpr uint32_t k_resolveFrontTextureUnit = 0u;
constexpr uint32_t k_resolveBackTextureUnit = 1u;

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

class ScopedDdpGlState
{
public:
  ScopedDdpGlState()
  {
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &m_drawFramebuffer);
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &m_readFramebuffer);
    glGetIntegerv(GL_VIEWPORT, m_viewport.data());
    glGetIntegerv(GL_SCISSOR_BOX, m_scissor.data());
    glGetIntegerv(GL_BLEND_SRC_RGB, &m_blendSrcRgb);
    glGetIntegerv(GL_BLEND_DST_RGB, &m_blendDstRgb);
    glGetIntegerv(GL_BLEND_SRC_ALPHA, &m_blendSrcAlpha);
    glGetIntegerv(GL_BLEND_DST_ALPHA, &m_blendDstAlpha);
    glGetIntegerv(GL_BLEND_EQUATION_RGB, &m_blendEquationRgb);
    glGetIntegerv(GL_BLEND_EQUATION_ALPHA, &m_blendEquationAlpha);
    m_blendEnabled = glIsEnabled(GL_BLEND);
    m_scissorEnabled = glIsEnabled(GL_SCISSOR_TEST);
    m_depthTestEnabled = glIsEnabled(GL_DEPTH_TEST);
    glGetBooleanv(GL_DEPTH_WRITEMASK, &m_depthMask);
  }

  ScopedDdpGlState(const ScopedDdpGlState&) = delete;
  ScopedDdpGlState& operator=(const ScopedDdpGlState&) = delete;

  ~ScopedDdpGlState()
  {
    restore();
    glBlendEquationSeparate(static_cast<GLenum>(m_blendEquationRgb), static_cast<GLenum>(m_blendEquationAlpha));
    glBlendFuncSeparate(
      static_cast<GLenum>(m_blendSrcRgb),
      static_cast<GLenum>(m_blendDstRgb),
      static_cast<GLenum>(m_blendSrcAlpha),
      static_cast<GLenum>(m_blendDstAlpha));
    m_blendEnabled ? glEnable(GL_BLEND) : glDisable(GL_BLEND);
    m_scissorEnabled ? glEnable(GL_SCISSOR_TEST) : glDisable(GL_SCISSOR_TEST);
    m_depthTestEnabled ? glEnable(GL_DEPTH_TEST) : glDisable(GL_DEPTH_TEST);
    glDepthMask(m_depthMask);
  }

  void restore() const noexcept
  {
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, static_cast<GLuint>(m_drawFramebuffer));
    glBindFramebuffer(GL_READ_FRAMEBUFFER, static_cast<GLuint>(m_readFramebuffer));
    glViewport(m_viewport[0], m_viewport[1], m_viewport[2], m_viewport[3]);
    glScissor(m_scissor[0], m_scissor[1], m_scissor[2], m_scissor[3]);
  }

private:
  GLint m_drawFramebuffer = 0;
  GLint m_readFramebuffer = 0;
  std::array<GLint, 4> m_viewport{};
  std::array<GLint, 4> m_scissor{};
  GLint m_blendSrcRgb = GL_ONE;
  GLint m_blendDstRgb = GL_ZERO;
  GLint m_blendSrcAlpha = GL_ONE;
  GLint m_blendDstAlpha = GL_ZERO;
  GLint m_blendEquationRgb = GL_FUNC_ADD;
  GLint m_blendEquationAlpha = GL_FUNC_ADD;
  GLboolean m_blendEnabled = GL_FALSE;
  GLboolean m_scissorEnabled = GL_FALSE;
  GLboolean m_depthTestEnabled = GL_FALSE;
  GLboolean m_depthMask = GL_TRUE;
};

void clearDdpTargets(MeshDdpResources& resources, const uint32_t textureId)
{
  const uint32_t attachmentOffset = 3u * textureId;
  resources.peelFbo().bind(fbo::TargetType::DrawAndRead);

  // Depth bounds are stored as (-nearestDepth, farthestDepth), then blended with GL_MAX so all fragments for a pixel
  // contribute to one min/max pair without needing atomics.
  glDrawBuffer(k_peelAttachments[attachmentOffset]);
  glClearColor(-1.0f, -1.0f, 0.0f, 0.0f);
  glClear(GL_COLOR_BUFFER_BIT);

  const std::array<GLenum, 2> colorAttachments{
    k_peelAttachments[attachmentOffset + 1u],
    k_peelAttachments[attachmentOffset + 2u]};
  glDrawBuffers(static_cast<GLsizei>(colorAttachments.size()), colorAttachments.data());
  glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
  glClear(GL_COLOR_BUFFER_BIT);
}

void clearAccumulatedBackColor(MeshDdpResources& resources)
{
  resources.backBlendFbo().bind(fbo::TargetType::DrawAndRead);
  glDrawBuffer(GL_COLOR_ATTACHMENT0);
  glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
  glClear(GL_COLOR_BUFFER_BIT);
}

void drawFullScreenTriangle(MeshDdpResources& resources)
{
  resources.fullScreenVao().bind();
  glDrawArrays(GL_TRIANGLES, 0, 3);
  resources.fullScreenVao().release();
}

void initializeDepthBounds(const MeshDdpRenderRequest& request)
{
  request.resources.peelFbo().bind(fbo::TargetType::DrawAndRead);
  glDrawBuffer(GL_COLOR_ATTACHMENT0);
  glEnable(GL_BLEND);
  glBlendEquation(GL_MAX);
  glBlendFunc(GL_ONE, GL_ONE);

  // The initialization shader writes only depth bounds. Opaque and translucent surface meshes are both included when
  // DDP is active so opaque alpha-1 fragments correctly occlude transparent fragments behind them.
  request.meshRenderer.drawBucket(request.renderables, request.context, request.initProgram);
  if (request.drawExtraDepthBounds) {
    request.drawExtraDepthBounds();
  }
}

void peelFrontAndBackLayers(const MeshDdpRenderRequest& request, const uint32_t currentId)
{
  const uint32_t previousId = 1u - currentId;
  const uint32_t attachmentOffset = 3u * currentId;
  const std::array<GLenum, 3> drawAttachments{
    k_peelAttachments[attachmentOffset],
    k_peelAttachments[attachmentOffset + 1u],
    k_peelAttachments[attachmentOffset + 2u]};

  request.resources.peelFbo().bind(fbo::TargetType::DrawAndRead);
  glDrawBuffers(static_cast<GLsizei>(drawAttachments.size()), drawAttachments.data());
  glEnable(GL_BLEND);
  glBlendEquation(GL_MAX);
  glBlendFunc(GL_ONE, GL_ONE);

  // Each peel reads the previous depth bounds and front-color accumulation, then writes the next bounds plus the newly
  // peeled front/back colors into the opposite ping-pong target.
  request.resources.depthTexture(previousId).bind(k_depthTextureUnit);
  request.resources.frontColorTexture(previousId).bind(k_frontColorTextureUnit);
  request.peelProgram.use();
  request.peelProgram.setUniform("u_previousDepthBoundsTex", static_cast<GLint>(k_depthTextureUnit));
  request.peelProgram.setUniform("u_previousFrontColorTex", static_cast<GLint>(k_frontColorTextureUnit));
  request.meshRenderer.drawBucket(request.renderables, request.context, request.peelProgram);
  request.resources.frontColorTexture(previousId).unbind(k_frontColorTextureUnit);
  request.resources.depthTexture(previousId).unbind(k_depthTextureUnit);
  if (request.drawExtraPeelLayers) {
    request.drawExtraPeelLayers(
      request.resources.depthTexture(previousId),
      request.resources.frontColorTexture(previousId));
  }
}

void blendBackLayer(const MeshDdpRenderRequest& request, const uint32_t currentId, const GLuint completionQuery)
{
  request.resources.backBlendFbo().bind(fbo::TargetType::DrawAndRead);
  glDrawBuffer(GL_COLOR_ATTACHMENT0);
  glEnable(GL_BLEND);
  glBlendEquation(GL_FUNC_ADD);
  glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

  // Back layers are accumulated furthest-to-nearest into a single texture. Colors are premultiplied in the peel shader.
  request.resources.backTempTexture(currentId).bind(k_backTempTextureUnit);
  request.backBlendProgram.use();
  request.backBlendProgram.setUniform("u_backTempTex", static_cast<GLint>(k_backTempTextureUnit));
  if (completionQuery != 0u) {
    glBeginQuery(GL_ANY_SAMPLES_PASSED, completionQuery);
  }
  drawFullScreenTriangle(request.resources);
  if (completionQuery != 0u) {
    glEndQuery(GL_ANY_SAMPLES_PASSED);
  }
  request.backBlendProgram.stopUse();
  request.resources.backTempTexture(currentId).unbind(k_backTempTextureUnit);
}

std::optional<bool> completedQueryHasSamples(const GLuint query)
{
  GLuint available = GL_FALSE;
  glGetQueryObjectuiv(query, GL_QUERY_RESULT_AVAILABLE, &available);
  if (available != GL_TRUE) {
    return std::nullopt;
  }

  GLuint anySamplesPassed = GL_FALSE;
  glGetQueryObjectuiv(query, GL_QUERY_RESULT, &anySamplesPassed);
  return anySamplesPassed == GL_TRUE;
}

void resolveDdp(const MeshDdpRenderRequest& request, const ScopedDdpGlState& scopedState, const uint32_t currentId)
{
  // Resolve into the framebuffer and viewport that were active before the DDP pass.
  scopedState.restore();
  glDisable(GL_DEPTH_TEST);
  glDepthMask(GL_FALSE);
  glEnable(GL_BLEND);
  glBlendEquation(GL_FUNC_ADD);
  glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

  request.resources.frontColorTexture(currentId).bind(k_resolveFrontTextureUnit);
  request.resources.backColorTexture().bind(k_resolveBackTextureUnit);
  request.resolveProgram.use();
  request.resolveProgram.setUniform("u_frontColorTex", static_cast<GLint>(k_resolveFrontTextureUnit));
  request.resolveProgram.setUniform("u_backColorTex", static_cast<GLint>(k_resolveBackTextureUnit));
  const GlViewport viewport = currentViewport();
  request.resolveProgram.setUniform("u_viewportOrigin", glm::ivec2{viewport.x, viewport.y});
  drawFullScreenTriangle(request.resources);
  request.resolveProgram.stopUse();
  request.resources.backColorTexture().unbind(k_resolveBackTextureUnit);
  request.resources.frontColorTexture(currentId).unbind(k_resolveFrontTextureUnit);
}

} // namespace

void renderMeshDdpAlphaOver(const MeshDdpRenderRequest& request)
{
  // Image planes are submitted through the extra draw callbacks rather than request.renderables. An active plan may
  // therefore have no ordinary mesh surfaces and must still execute the complete DDP pass.
  if (!request.plan.active) {
    return;
  }

  const ScopedDdpGlState scopedState;
  const GlViewport originalViewport = currentViewport();
  const glm::uvec2 size = viewportSize(originalViewport);
  if (!request.resources.ensureSize(size) && !request.resources.initialized()) {
    return;
  }

  glViewport(0, 0, static_cast<GLsizei>(size.x), static_cast<GLsizei>(size.y));
  glDisable(GL_SCISSOR_TEST);
  glDisable(GL_DEPTH_TEST);
  glDepthMask(GL_FALSE);

  clearAccumulatedBackColor(request.resources);
  clearDdpTargets(request.resources, 0u);
  initializeDepthBounds(request);

  std::vector<GLuint> completionQueries;
  if (request.plan.untilComplete) {
    completionQueries.resize(request.plan.peelPasses, 0u);
    glGenQueries(static_cast<GLsizei>(completionQueries.size()), completionQueries.data());
  }

  uint32_t currentId = 0u;
  uint32_t completedPasses = 0u;
  uint32_t nextQueryToPoll = 0u;
  bool transparencyComplete = false;
  while (request.plan.active && completedPasses < request.plan.peelPasses && !transparencyComplete) {
    while (!completionQueries.empty() && nextQueryToPoll < completedPasses) {
      const std::optional<bool> hasSamples = completedQueryHasSamples(completionQueries[nextQueryToPoll]);
      if (!hasSamples) {
        break;
      }
      ++nextQueryToPoll;
      if (!*hasSamples) {
        transparencyComplete = true;
        break;
      }
    }
    if (transparencyComplete) {
      break;
    }

    currentId = (completedPasses + 1u) % 2u;
    clearDdpTargets(request.resources, currentId);
    peelFrontAndBackLayers(request, currentId);
    blendBackLayer(request, currentId, completionQueries.empty() ? 0u : completionQueries[completedPasses]);
    ++completedPasses;
  }

  if (!completionQueries.empty()) {
    glDeleteQueries(static_cast<GLsizei>(completionQueries.size()), completionQueries.data());
  }

  resolveDdp(request, scopedState, currentId);
}

} // namespace rendering::mesh
