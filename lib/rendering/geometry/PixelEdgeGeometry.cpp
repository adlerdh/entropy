#include "rendering/geometry/PixelEdgeGeometry.h"

#include <algorithm>
#include <cmath>

namespace rendering::pixel_edge
{

namespace
{

struct PixelInterval
{
  int begin;
  int end;
};

PixelInterval clipIntervalToPixels(float clipOrigin, float clipExtent, float deviceExtent)
{
  const float first = (clipOrigin * 0.5f + 0.5f) * deviceExtent;
  const float second = ((clipOrigin + clipExtent) * 0.5f + 0.5f) * deviceExtent;
  const int deviceEnd = std::max(0, static_cast<int>(std::lround(deviceExtent)));
  const int begin = std::clamp(static_cast<int>(std::lround(std::min(first, second))), 0, deviceEnd);
  const int end = std::clamp(static_cast<int>(std::lround(std::max(first, second))), begin, deviceEnd);
  return {begin, end};
}

} // namespace

ViewRect computeViewRect(glm::vec4 clipViewport, glm::vec4 deviceViewport)
{
  const PixelInterval x = clipIntervalToPixels(clipViewport.x, clipViewport.z, deviceViewport.z);
  const PixelInterval y = clipIntervalToPixels(clipViewport.y, clipViewport.w, deviceViewport.w);
  const int windowOriginX = static_cast<int>(std::lround(deviceViewport.x));
  const int windowOriginY = static_cast<int>(std::lround(deviceViewport.y));

  return ViewRect{x.begin, y.begin, windowOriginX + x.begin, windowOriginY + y.begin, x.end - x.begin, y.end - y.begin};
}

} // namespace rendering::pixel_edge
