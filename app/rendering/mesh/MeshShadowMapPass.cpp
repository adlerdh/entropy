#include "rendering/mesh/MeshShadowMapPass.h"

#include "rendering/mesh/MeshShadowMapResources.h"
#include "rendering/utility/gl/GLFrameBufferObject.h"

#include <glad/glad.h>

#include <array>
#include <cstdint>

namespace rendering::mesh
{

namespace
{

class ScopedShadowMapGlState
{
public:
  ScopedShadowMapGlState()
  {
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &m_drawFramebuffer);
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &m_readFramebuffer);
    glGetIntegerv(GL_VIEWPORT, m_viewport.data());
    glGetIntegerv(GL_SCISSOR_BOX, m_scissor.data());
    glGetBooleanv(GL_COLOR_WRITEMASK, m_colorMask.data());
    glGetBooleanv(GL_DEPTH_WRITEMASK, &m_depthMask);
    glGetIntegerv(GL_DEPTH_FUNC, &m_depthFunction);
    m_scissorEnabled = glIsEnabled(GL_SCISSOR_TEST);
    m_depthTestEnabled = glIsEnabled(GL_DEPTH_TEST);
    m_cullFaceEnabled = glIsEnabled(GL_CULL_FACE);
    m_polygonOffsetFillEnabled = glIsEnabled(GL_POLYGON_OFFSET_FILL);
    glGetIntegerv(GL_CULL_FACE_MODE, &m_cullFaceMode);
    glGetFloatv(GL_POLYGON_OFFSET_FACTOR, &m_polygonOffsetFactor);
    glGetFloatv(GL_POLYGON_OFFSET_UNITS, &m_polygonOffsetUnits);
  }

  ScopedShadowMapGlState(const ScopedShadowMapGlState&) = delete;
  ScopedShadowMapGlState& operator=(const ScopedShadowMapGlState&) = delete;

  ~ScopedShadowMapGlState()
  {
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, static_cast<GLuint>(m_drawFramebuffer));
    glBindFramebuffer(GL_READ_FRAMEBUFFER, static_cast<GLuint>(m_readFramebuffer));
    glViewport(m_viewport[0], m_viewport[1], m_viewport[2], m_viewport[3]);
    glScissor(m_scissor[0], m_scissor[1], m_scissor[2], m_scissor[3]);
    glColorMask(m_colorMask[0], m_colorMask[1], m_colorMask[2], m_colorMask[3]);
    glDepthFunc(static_cast<GLenum>(m_depthFunction));
    glDepthMask(m_depthMask);
    glPolygonOffset(m_polygonOffsetFactor, m_polygonOffsetUnits);
    m_scissorEnabled ? glEnable(GL_SCISSOR_TEST) : glDisable(GL_SCISSOR_TEST);
    m_depthTestEnabled ? glEnable(GL_DEPTH_TEST) : glDisable(GL_DEPTH_TEST);
    m_cullFaceEnabled ? glEnable(GL_CULL_FACE) : glDisable(GL_CULL_FACE);
    m_polygonOffsetFillEnabled ? glEnable(GL_POLYGON_OFFSET_FILL) : glDisable(GL_POLYGON_OFFSET_FILL);
    glCullFace(static_cast<GLenum>(m_cullFaceMode));
  }

private:
  GLint m_drawFramebuffer = 0;
  GLint m_readFramebuffer = 0;
  std::array<GLint, 4> m_viewport{};
  std::array<GLint, 4> m_scissor{};
  std::array<GLboolean, 4> m_colorMask{};
  GLboolean m_depthMask = GL_TRUE;
  GLint m_depthFunction = GL_LESS;
  GLboolean m_scissorEnabled = GL_FALSE;
  GLboolean m_depthTestEnabled = GL_FALSE;
  GLboolean m_cullFaceEnabled = GL_FALSE;
  GLboolean m_polygonOffsetFillEnabled = GL_FALSE;
  GLint m_cullFaceMode = GL_BACK;
  GLfloat m_polygonOffsetFactor = 0.0f;
  GLfloat m_polygonOffsetUnits = 0.0f;
};

} // namespace

bool renderMeshShadowMap(const MeshShadowMapRenderRequest& request)
{
  if (
    request.plan.state != MeshAdvancedLightingFeatureState::Enabled || request.plan.mapSizePixels == 0u ||
    request.renderables.empty())
  {
    return false;
  }

  const ScopedShadowMapGlState scopedState;
  request.resources.ensureSize(request.plan.mapSizePixels);
  request.resources.fbo().bind(fbo::TargetType::DrawAndRead);

  const auto size = static_cast<GLsizei>(request.plan.mapSizePixels);
  glViewport(0, 0, size, size);
  glDisable(GL_SCISSOR_TEST);
  glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_LESS);
  glDepthMask(GL_TRUE);
  glEnable(GL_POLYGON_OFFSET_FILL);
  glPolygonOffset(1.5f, 2.0f);
  glClearDepth(1.0);
  glClear(GL_DEPTH_BUFFER_BIT);

  request.meshRenderer.drawBucket(request.renderables, request.context, request.depthProgram);
  return true;
}

} // namespace rendering::mesh
