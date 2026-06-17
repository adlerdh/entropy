#pragma once

#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>

namespace viewer
{

/**
 * @brief View bounds and clip-space transforms for one frame.
 */
class FrameViewport
{
public:
  FrameViewport();
  explicit FrameViewport(glm::vec4 windowClipViewport);

  /** @brief Set viewport bounds in enclosing-window clip coordinates. */
  void setWindowClipViewport(glm::vec4 windowClipViewport);

  /** @brief Viewport bounds in enclosing-window clip coordinates. */
  const glm::vec4& windowClipViewport() const;

  /** @brief Transform from this frame's clip coordinates to enclosing-window clip coordinates. */
  const glm::mat4& windowClip_T_viewClip() const;

  /** @brief Transform from enclosing-window clip coordinates to this frame's clip coordinates. */
  const glm::mat4& viewClip_T_windowClip() const;

private:
  void updateTransforms();

  glm::vec4 m_windowClipViewport;    //!< Bounds in enclosing-window clip coordinates.
  glm::mat4 m_windowClip_T_viewClip; //!< View clip to enclosing-window clip transform.
  glm::mat4 m_viewClip_T_windowClip; //!< Enclosing-window clip to view clip transform.
};

} // namespace viewer
