#include "viewer_types/FrameViewport.h"

#include <glm/ext/matrix_transform.hpp>
#include <glm/glm.hpp>

#include <utility>

namespace viewer_types
{
namespace
{

constexpr glm::vec4 sk_fullWindowClipViewport{-1.0f, -1.0f, 2.0f, 2.0f};

glm::mat4 computeWindowClip_T_viewClip(const glm::vec4& windowClipViewport)
{
  const glm::vec3 translation{
    windowClipViewport[0] + 0.5f * windowClipViewport[2],
    windowClipViewport[1] + 0.5f * windowClipViewport[3],
    0.0f};

  const glm::vec3 scale{0.5f * windowClipViewport[2], 0.5f * windowClipViewport[3], 1.0f};

  return glm::translate(glm::mat4{1.0f}, translation) * glm::scale(glm::mat4{1.0f}, scale);
}

} // namespace

FrameViewport::FrameViewport() : FrameViewport(sk_fullWindowClipViewport) {}

FrameViewport::FrameViewport(glm::vec4 windowClipViewport)
  : m_windowClipViewport(std::move(windowClipViewport)), m_windowClip_T_viewClip(1.0f), m_viewClip_T_windowClip(1.0f)
{
  updateTransforms();
}

void FrameViewport::setWindowClipViewport(glm::vec4 windowClipViewport)
{
  m_windowClipViewport = std::move(windowClipViewport);
  updateTransforms();
}

const glm::vec4& FrameViewport::windowClipViewport() const
{
  return m_windowClipViewport;
}

const glm::mat4& FrameViewport::windowClip_T_viewClip() const
{
  return m_windowClip_T_viewClip;
}

const glm::mat4& FrameViewport::viewClip_T_windowClip() const
{
  return m_viewClip_T_windowClip;
}

void FrameViewport::updateTransforms()
{
  m_windowClip_T_viewClip = computeWindowClip_T_viewClip(m_windowClipViewport);
  m_viewClip_T_windowClip = glm::inverse(m_windowClip_T_viewClip);
}

} // namespace viewer_types
