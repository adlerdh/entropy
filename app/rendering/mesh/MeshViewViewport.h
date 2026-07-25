#pragma once

#include <glm/common.hpp>
#include <glm/vec4.hpp>

#include <algorithm>
#include <array>
#include <cmath>

class View;
class WindowData;

namespace rendering::mesh
{

/**
 * @brief Convert a view rectangle from window clip coordinates to device-pixel viewport coordinates
 * @param windowClipViewport View rectangle in the enclosing window's clip coordinate space
 * @param windowDeviceViewport Enclosing window viewport as {left, bottom, width, height} in device pixels
 * @return View viewport as {left, bottom, width, height} in device pixels
 */
inline glm::ivec4 meshViewDeviceViewport(const glm::vec4& windowClipViewport, const glm::vec4& windowDeviceViewport)
{
  const float left = windowDeviceViewport.x + 0.5f * (windowClipViewport.x + 1.0f) * windowDeviceViewport.z;
  const float bottom = windowDeviceViewport.y + 0.5f * (windowClipViewport.y + 1.0f) * windowDeviceViewport.w;
  const float width = 0.5f * windowClipViewport.z * windowDeviceViewport.z;
  const float height = 0.5f * windowClipViewport.w * windowDeviceViewport.w;

  return glm::ivec4{
    static_cast<int>(std::lround(left)),
    static_cast<int>(std::lround(bottom)),
    static_cast<int>(std::lround(std::max(0.0f, width))),
    static_cast<int>(std::lround(std::max(0.0f, height)))};
}

/**
 * @brief Install a view-sized OpenGL viewport and scissor rectangle for a mesh draw scope
 *
 * Mesh shaders write ordinary view clip coordinates. This guard maps those coordinates into the selected view by
 * setting the OpenGL viewport itself, instead of baking the view's window placement into every mesh projection matrix.
 */
class ScopedMeshViewViewport
{
public:
  /**
   * @brief Save the current viewport/scissor state and install the viewport for one view
   * @param view View receiving mesh rendering
   * @param windowData Window containing the view
   */
  ScopedMeshViewViewport(const View& view, const WindowData& windowData);

  ScopedMeshViewViewport(const ScopedMeshViewViewport&) = delete;
  ScopedMeshViewViewport& operator=(const ScopedMeshViewViewport&) = delete;

  /** @brief Restore the viewport and scissor state active before construction */
  ~ScopedMeshViewViewport();

private:
  std::array<int, 4> m_previousViewport{};
  std::array<int, 4> m_previousScissor{};
  unsigned char m_previousScissorEnabled = 0;
};

} // namespace rendering::mesh
