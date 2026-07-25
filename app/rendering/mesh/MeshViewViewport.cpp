#include "rendering/mesh/MeshViewViewport.h"

#include "windowing/View.h"
#include "windowing/WindowData.h"

#include <glad/glad.h>

namespace rendering::mesh
{

ScopedMeshViewViewport::ScopedMeshViewViewport(const View& view, const WindowData& windowData)
{
  glGetIntegerv(GL_VIEWPORT, m_previousViewport.data());
  glGetIntegerv(GL_SCISSOR_BOX, m_previousScissor.data());
  m_previousScissorEnabled = glIsEnabled(GL_SCISSOR_TEST);

  const glm::ivec4 viewViewport =
    meshViewDeviceViewport(view.windowClipViewport(), windowData.viewport().getDeviceAsVec4());
  glViewport(viewViewport.x, viewViewport.y, viewViewport.z, viewViewport.w);
  glEnable(GL_SCISSOR_TEST);
  glScissor(viewViewport.x, viewViewport.y, viewViewport.z, viewViewport.w);
}

ScopedMeshViewViewport::~ScopedMeshViewViewport()
{
  glViewport(m_previousViewport[0], m_previousViewport[1], m_previousViewport[2], m_previousViewport[3]);
  glScissor(m_previousScissor[0], m_previousScissor[1], m_previousScissor[2], m_previousScissor[3]);
  m_previousScissorEnabled ? glEnable(GL_SCISSOR_TEST) : glDisable(GL_SCISSOR_TEST);
}

} // namespace rendering::mesh
