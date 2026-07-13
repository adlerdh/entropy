#pragma once

#include <glm/vec4.hpp>

namespace rendering::pixel_edge
{

/**
 * @brief Pixel rectangle used by the pixel-edge post-process pass.
 *
 * Scene coordinates are relative to the offscreen scene texture. Window coordinates
 * are relative to the default framebuffer.
 */
struct ViewRect
{
  int sceneX = 0;  //!< View left in scene-texture pixels.
  int sceneY = 0;  //!< View bottom in scene-texture pixels.
  int windowX = 0; //!< View left in default-framebuffer pixels.
  int windowY = 0; //!< View bottom in default-framebuffer pixels.
  int width = 0;   //!< View width in device pixels.
  int height = 0;  //!< View height in device pixels.
};

/**
 * @brief Convert a view's clip-space viewport into scene and framebuffer pixels.
 * @param clipViewport View rectangle in normalized window-clip coordinates.
 * @param deviceViewport Main render viewport in device pixels.
 * @return Pixel rectangle for offscreen capture and default-framebuffer compositing.
 */
ViewRect computeViewRect(glm::vec4 clipViewport, glm::vec4 deviceViewport);

} // namespace rendering::pixel_edge
