#include "rendering/mesh/MeshShadowMapPass.h"

#include "rendering/mesh/MeshShadowMapResources.h"
#include "rendering/utility/gl/GLFrameBufferObject.h"
#include "rendering/utility/gl/OpenGLStateGuard.h"

#include <glad/glad.h>

#include <cstdint>

namespace rendering::mesh
{

bool renderMeshShadowMap(const MeshShadowMapRenderRequest& request)
{
  if (
    request.plan.state != MeshAdvancedLightingFeatureState::Enabled || request.plan.mapSizePixels == 0u ||
    request.renderables.empty())
  {
    return false;
  }

  const OpenGLStateGuard scopedState;
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

  request.meshRenderer
    .drawBucket(request.renderables, request.context, request.depthProgram, MeshDrawPass::ShadowDepth);
  return true;
}

} // namespace rendering::mesh
