#include "rendering/helpers/VectorDrawingHelpers.h"

#include "common/Viewport.h"

#include <glm/common.hpp>
#include <glm/geometric.hpp>

#include <algorithm>
#include <cmath>
#include <functional>

namespace entropy::rendering::vector_drawing
{

bool isFiniteVec2(const glm::vec2& value)
{
  return std::isfinite(value.x) && std::isfinite(value.y);
}

bool isInsideRect(const glm::vec2& value, const glm::vec2& min, const glm::vec2& size)
{
  return value.x >= min.x && value.y >= min.y && value.x < min.x + size.x && value.y < min.y + size.y;
}

std::optional<ArrowGeometry> computeArrowGeometry(const glm::vec2& start, const glm::vec2& end, float width)
{
  const glm::vec2 delta = end - start;
  const float length = glm::length(delta);
  if (length < 2.0f) {
    return std::nullopt;
  }

  const glm::vec2 direction = delta / length;
  const glm::vec2 normal{-direction.y, direction.x};
  const float headLength = glm::clamp(0.32f * length, std::max(5.0f, 3.0f * width), std::max(10.0f, 5.0f * width));
  const float headWidth = std::max(0.55f * headLength, 2.25f * width);
  const glm::vec2 headBase = end - headLength * direction;
  const glm::vec2 shaftEnd = end - std::max(0.5f * width, 1.0f) * direction;

  return ArrowGeometry{
    .shaftStart = start,
    .shaftEnd = shaftEnd,
    .headTip = end,
    .headLeft = headBase + headWidth * normal,
    .headRight = headBase - headWidth * normal};
}

glm::vec2 checkerCoordForViewClip(const glm::vec2& viewClipPos, float numCheckers, float aspectRatio)
{
  const glm::vec2 checkerBase = numCheckers * 0.5f * (viewClipPos + glm::vec2{1.0f});
  return glm::mix(
    glm::vec2{checkerBase.x, checkerBase.y / aspectRatio},
    glm::vec2{checkerBase.x * aspectRatio, checkerBase.y},
    aspectRatio <= 1.0f ? 1.0f : 0.0f);
}

bool shouldRenderFixedComparisonSample(
  ViewRenderMode renderMode,
  const glm::vec2& viewClipPos,
  const glm::vec2& checkerCoord,
  const glm::vec2& clipCrosshairs,
  const glm::ivec2& quadrants,
  bool showFixedImage,
  float aspectRatio,
  float flashlightRadius,
  bool flashlightMovingOnFixed)
{
  if (ViewRenderMode::Image == renderMode) {
    return true;
  }

  if (ViewRenderMode::Checkerboard == renderMode) {
    const bool checkerShowsFixed = std::fmod(std::floor(checkerCoord.x) + std::floor(checkerCoord.y), 2.0f) > 0.5f;
    return showFixedImage == checkerShowsFixed;
  }

  if (ViewRenderMode::Quadrants == renderMode) {
    const glm::bvec2 quadrant{viewClipPos.x <= clipCrosshairs.x, viewClipPos.y > clipCrosshairs.y};
    const bool quadrantShowsFixed =
      ((!static_cast<bool>(quadrants.x) || quadrant.x) == (!static_cast<bool>(quadrants.y) || quadrant.y));
    return showFixedImage == quadrantShowsFixed;
  }

  if (ViewRenderMode::Flashlight == renderMode) {
    const glm::vec2 delta{aspectRatio * (viewClipPos.x - clipCrosshairs.x), viewClipPos.y - clipCrosshairs.y};
    const float flashlightDistance = glm::length(delta);
    return (showFixedImage == (flashlightDistance > flashlightRadius)) || (flashlightMovingOnFixed && showFixedImage);
  }

  return false;
}

glm::vec2
projectWorldToMiewport(const Viewport& windowViewport, const glm::mat4& windowClip_T_world, const glm::vec3& worldPos)
{
  const glm::vec4 windowClipPos = windowClip_T_world * glm::vec4{worldPos, 1.0f};
  const glm::vec2 normalizedWindowClipPos{windowClipPos / windowClipPos.w};
  const glm::vec2 viewportPos{
    (normalizedWindowClipPos.x + 1.0f) * windowViewport.width() / 2.0f,
    (normalizedWindowClipPos.y + 1.0f) * windowViewport.height() / 2.0f};
  return glm::vec2{viewportPos.x, windowViewport.height() - viewportPos.y};
}

glm::vec2 viewClipFromMiewport(
  const Viewport& windowViewport,
  const glm::mat4& viewClip_T_windowClip,
  const glm::vec2& miewportPos)
{
  const glm::vec2 viewportPos{miewportPos.x, windowViewport.height() - miewportPos.y};
  const glm::vec2 windowClipPos{
    2.0f * viewportPos.x / windowViewport.width() - 1.0f,
    2.0f * viewportPos.y / windowViewport.height() - 1.0f};
  glm::vec4 viewClipPos = viewClip_T_windowClip * glm::vec4{windowClipPos, 0.0f, 1.0f};
  viewClipPos /= viewClipPos.w;
  return glm::vec2{viewClipPos};
}

float screenDistanceFromMiewportPositions(const glm::vec2& a, const glm::vec2& b)
{
  if (!isFiniteVec2(a) || !isFiniteVec2(b)) {
    return 0.0f;
  }
  return glm::length(b - a);
}

float meanScreenPixelsPerMillimeter(float rightPx, float upPx)
{
  const float meanPx = 0.5f * (rightPx + upPx);
  return meanPx > 0.0f ? meanPx : 1.0f;
}

float meanInPlaneScreenPixelsPerVoxel(std::array<float, 3> axisLengths)
{
  std::sort(axisLengths.begin(), axisLengths.end(), std::greater<float>{});
  const float meanInPlanePx = 0.5f * (axisLengths[0] + axisLengths[1]);
  return meanInPlanePx > 0.0f ? meanInPlanePx : 1.0f;
}

float vectorArrowSpacingPixels(
  VectorArrowOverlaySpacingMode spacingMode,
  float pixelSpacing,
  float voxelSpacing,
  float millimeterSpacing,
  float screenPixelsPerVoxel,
  float screenPixelsPerMillimeter)
{
  switch (spacingMode) {
    case VectorArrowOverlaySpacingMode::Pixels:
      return std::max(pixelSpacing, 0.1f);
    case VectorArrowOverlaySpacingMode::Voxels:
      return std::max(voxelSpacing, 0.1f) * screenPixelsPerVoxel;
    case VectorArrowOverlaySpacingMode::Millimeters:
      return std::max(millimeterSpacing, 0.1f) * screenPixelsPerMillimeter;
  }

  return std::max(pixelSpacing, 0.1f);
}

} // namespace entropy::rendering::vector_drawing
