#include "rendering/mesh/MeshDdpPass.h"

#include "rendering/mesh/MeshDdpResources.h"
#include "rendering/utility/gl/GLFrameBufferObject.h"
#include "rendering/utility/gl/GLErrorChecker.h"
#include "rendering/utility/gl/GLShaderProgram.h"
#include "rendering/utility/gl/GLTexture.h"
#include "rendering/utility/gl/GLVertexArrayObject.h"
#include "rendering/utility/gl/OpenGLStateGuard.h"

#include "common/Exception.hpp"

#include <glad/glad.h>

#include <glm/vec2.hpp>

#include <array>
#include <cstdint>
#include <limits>
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

std::size_t validatedQueryCount(const uint32_t count)
{
  if (count > static_cast<uint32_t>(std::numeric_limits<GLsizei>::max())) {
    throwDebug("DDP query count exceeds the OpenGL object-count range");
  }
  return count;
}

class QueryObjects
{
public:
  explicit QueryObjects(const uint32_t count) : m_ids(validatedQueryCount(count), 0u)
  {
    if (!m_ids.empty()) {
      glGenQueries(static_cast<GLsizei>(m_ids.size()), m_ids.data());
      CHECK_GL_ERROR(GLErrorChecker{});
    }
  }

  ~QueryObjects()
  {
    if (!m_ids.empty()) {
      glDeleteQueries(static_cast<GLsizei>(m_ids.size()), m_ids.data());
    }
  }

  QueryObjects(const QueryObjects&) = delete;
  QueryObjects& operator=(const QueryObjects&) = delete;

  [[nodiscard]] bool empty() const noexcept
  {
    return m_ids.empty();
  }
  [[nodiscard]] GLuint operator[](const std::size_t index) const
  {
    return m_ids.at(index);
  }

private:
  std::vector<GLuint> m_ids;
};

class ActiveSamplesPassedQuery
{
public:
  explicit ActiveSamplesPassedQuery(const GLuint query)
  {
    GLint activeQuery = 0;
    glGetQueryiv(GL_ANY_SAMPLES_PASSED, GL_CURRENT_QUERY, &activeQuery);
    if (activeQuery != 0) {
      throwDebug("Cannot begin a DDP completion query while another samples-passed query is active");
    }
    // Attribute any queued error to the call that produced it instead of risking a successful begin followed by a
    // constructor exception that would leave the query active.
    CHECK_GL_ERROR(GLErrorChecker{});
    glBeginQuery(GL_ANY_SAMPLES_PASSED, query);
    CHECK_GL_ERROR(GLErrorChecker{});
    m_active = true;
  }

  ~ActiveSamplesPassedQuery()
  {
    if (m_active) {
      glEndQuery(GL_ANY_SAMPLES_PASSED);
    }
  }

  ActiveSamplesPassedQuery(const ActiveSamplesPassedQuery&) = delete;
  ActiveSamplesPassedQuery& operator=(const ActiveSamplesPassedQuery&) = delete;

  void finish()
  {
    if (!m_active) {
      return;
    }
    glEndQuery(GL_ANY_SAMPLES_PASSED);
    m_active = false;
    CHECK_GL_ERROR(GLErrorChecker{});
  }

private:
  bool m_active = false;
};

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
  resources.fullScreenVao().drawArrays(PrimitiveMode::Triangles, 0, 3);
  resources.fullScreenVao().unbind();
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
  request.meshRenderer.drawBucket(request.renderables, request.context, request.initProgram, MeshDrawPass::DepthBounds);
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
  request.peelProgram.setSamplerUniform("u_previousDepthBoundsTex", static_cast<GLint>(k_depthTextureUnit));
  request.peelProgram.setSamplerUniform("u_previousFrontColorTex", static_cast<GLint>(k_frontColorTextureUnit));
  request.meshRenderer.drawBucket(request.renderables, request.context, request.peelProgram);
  request.resources.frontColorTexture(previousId).unbind(k_frontColorTextureUnit);
  request.resources.depthTexture(previousId).unbind(k_depthTextureUnit);
  if (request.drawExtraPeelLayers) {
    request.drawExtraPeelLayers(
      request.resources.depthTexture(previousId),
      request.resources.frontColorTexture(previousId));
  }
}

void blendBackLayer(const MeshDdpRenderRequest& request, const uint32_t currentId)
{
  request.resources.backBlendFbo().bind(fbo::TargetType::DrawAndRead);
  glDrawBuffer(GL_COLOR_ATTACHMENT0);
  glEnable(GL_BLEND);
  glBlendEquation(GL_FUNC_ADD);
  glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

  // Back layers are accumulated furthest-to-nearest into a single texture. Colors are premultiplied in the peel shader.
  request.resources.backTempTexture(currentId).bind(k_backTempTextureUnit);
  request.backBlendProgram.use();
  request.backBlendProgram.setSamplerUniform("u_backTempTex", static_cast<GLint>(k_backTempTextureUnit));
  drawFullScreenTriangle(request.resources);
  request.backBlendProgram.stopUse();
  request.resources.backTempTexture(currentId).unbind(k_backTempTextureUnit);
}

void queryRemainingLayers(const MeshDdpRenderRequest& request, const uint32_t currentId, const GLuint completionQuery)
{
  if (completionQuery == 0u) {
    return;
  }

  // Completion is determined from the next min/max depth bounds, not from back-layer alpha. A transparent boundary
  // can contribute no color while deeper geometry still remains, so using back alpha can stop peeling too early.
  request.resources.backBlendFbo().bind(fbo::TargetType::DrawAndRead);
  request.resources.depthTexture(currentId).bind(k_depthTextureUnit);
  request.completionProgram.use();
  request.completionProgram.setSamplerUniform("u_depthBoundsTex", static_cast<GLint>(k_depthTextureUnit));
  {
    ActiveSamplesPassedQuery query(completionQuery);
    drawFullScreenTriangle(request.resources);
    query.finish();
  }
  request.completionProgram.stopUse();
  request.resources.depthTexture(currentId).unbind(k_depthTextureUnit);
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
  CHECK_GL_ERROR(GLErrorChecker{});
  return anySamplesPassed == GL_TRUE;
}

void resolveDdp(const MeshDdpRenderRequest& request, const OpenGLStateGuard& scopedState, const uint32_t currentId)
{
  // Resolve into the framebuffer and viewport that were active before the DDP pass.
  scopedState.restoreFramebufferAndViewport();
  glDisable(GL_DEPTH_TEST);
  glDepthMask(GL_FALSE);
  glEnable(GL_BLEND);
  glBlendEquation(GL_FUNC_ADD);
  glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

  request.resources.frontColorTexture(currentId).bind(k_resolveFrontTextureUnit);
  request.resources.backColorTexture().bind(k_resolveBackTextureUnit);
  request.resolveProgram.use();
  request.resolveProgram.setSamplerUniform("u_frontColorTex", static_cast<GLint>(k_resolveFrontTextureUnit));
  request.resolveProgram.setSamplerUniform("u_backColorTex", static_cast<GLint>(k_resolveBackTextureUnit));
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

  const OpenGLStateGuard scopedState{
    {0u, GL_TEXTURE_2D},
    {0u, GL_TEXTURE_3D},
    {1u, GL_TEXTURE_2D},
    {1u, GL_TEXTURE_3D},
    {2u, GL_TEXTURE_2D},
    {2u, GL_TEXTURE_3D},
    {3u, GL_TEXTURE_2D},
    {3u, GL_TEXTURE_3D},
    {4u, GL_TEXTURE_1D},
    {5u, GL_TEXTURE_2D},
    {5u, GL_TEXTURE_3D},
    {6u, GL_TEXTURE_2D},
    {6u, GL_TEXTURE_BUFFER},
    {7u, GL_TEXTURE_2D},
    {8u, GL_TEXTURE_2D}};
  const GlViewport originalViewport = currentViewport();
  const glm::uvec2 size = viewportSize(originalViewport);
  if (!request.resources.ensureSize(size) && !request.resources.initialized()) {
    return;
  }

  glViewport(0, 0, static_cast<GLsizei>(size.x), static_cast<GLsizei>(size.y));
  glDisable(GL_SCISSOR_TEST);
  glDisable(GL_DEPTH_TEST);
  glDisable(GL_STENCIL_TEST);
  glDisable(GL_CULL_FACE);
  glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
  glDepthMask(GL_FALSE);

  clearAccumulatedBackColor(request.resources);
  clearDdpTargets(request.resources, 0u);
  initializeDepthBounds(request);

  const QueryObjects completionQueries(request.plan.peelPasses);

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
    blendBackLayer(request, currentId);
    queryRemainingLayers(request, currentId, completionQueries.empty() ? 0u : completionQueries[completedPasses]);
    ++completedPasses;
  }

  resolveDdp(request, scopedState, currentId);
}

} // namespace rendering::mesh
