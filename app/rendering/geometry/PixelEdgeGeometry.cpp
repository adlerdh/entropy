#include "rendering/geometry/PixelEdgeGeometry.h"

namespace entropy::rendering::pixel_edge
{

ViewRect computeViewRect(glm::vec4 clipViewport, glm::vec4 deviceViewport)
{
  const int sceneX = static_cast<int>((clipViewport.x * 0.5f + 0.5f) * deviceViewport.z);
  const int sceneY = static_cast<int>((clipViewport.y * 0.5f + 0.5f) * deviceViewport.w);
  const int width = static_cast<int>(clipViewport.z * 0.5f * deviceViewport.z);
  const int height = static_cast<int>(clipViewport.w * 0.5f * deviceViewport.w);

  return ViewRect{
    sceneX,
    sceneY,
    static_cast<int>(deviceViewport.x) + sceneX,
    static_cast<int>(deviceViewport.y) + sceneY,
    width,
    height};
}

} // namespace entropy::rendering::pixel_edge
