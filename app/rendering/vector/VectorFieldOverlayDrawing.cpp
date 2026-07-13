#include "rendering/vector/VectorFieldOverlayDrawing.h"

#include "image/Image.h"
#include "logic/camera/CameraHelpers.h"
#include "windowing/View.h"

#include <glm/glm.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <functional>

namespace
{

float screenDistance(const Viewport& windowViewport, const View& view, const glm::vec3& worldA, const glm::vec3& worldB)
{
  const glm::vec2 a = helper::miewport_T_world(windowViewport, view.camera(), view.windowClip_T_viewClip(), worldA);
  const glm::vec2 b = helper::miewport_T_world(windowViewport, view.camera(), view.windowClip_T_viewClip(), worldB);
  if (!std::isfinite(a.x) || !std::isfinite(a.y) || !std::isfinite(b.x) || !std::isfinite(b.y)) {
    return 0.0f;
  }
  return glm::length(b - a);
}

} // namespace

namespace rendering::vector_overlay
{

glm::vec3 subjectViewDirection(const Image& image, const View& view, const Directions::View direction)
{
  const glm::vec3 worldViewDirection = helper::worldDirection(view.camera(), direction);
  return glm::normalize(glm::mat3{image.transformations().subject_T_worldDef()} * worldViewDirection);
}

float screenPixelsPerMillimeter(const Viewport& windowViewport, const View& view, const glm::vec3& worldCrosshairs)
{
  const glm::vec3 worldRight = helper::worldDirection(view.camera(), Directions::View::Right);
  const glm::vec3 worldUp = helper::worldDirection(view.camera(), Directions::View::Up);
  const float rightPx = screenDistance(windowViewport, view, worldCrosshairs, worldCrosshairs + worldRight);
  const float upPx = screenDistance(windowViewport, view, worldCrosshairs, worldCrosshairs + worldUp);
  const float meanPx = 0.5f * (rightPx + upPx);
  return meanPx > 0.0f ? meanPx : 1.0f;
}

float screenPixelsPerVoxel(
  const Viewport& windowViewport,
  const View& view,
  const Image& image,
  const glm::vec3& worldCrosshairs)
{
  const glm::mat4 subject_T_world = image.transformations().subject_T_worldDef();
  const glm::mat4 pixel_T_subject = image.transformations().pixel_T_subject();
  const glm::mat4 subject_T_pixel = image.transformations().subject_T_pixel();
  const glm::mat4 world_T_subject = image.transformations().worldDef_T_subject();

  const glm::vec3 subjectBase{subject_T_world * glm::vec4{worldCrosshairs, 1.0f}};
  const glm::vec3 pixelBase{pixel_T_subject * glm::vec4{subjectBase, 1.0f}};
  const glm::vec3 worldBase{world_T_subject * glm::vec4{subjectBase, 1.0f}};

  std::array<float, 3> axisLengths{};
  for (int axis = 0; axis < 3; ++axis) {
    glm::vec3 pixelEnd = pixelBase;
    pixelEnd[axis] += 1.0f;
    const glm::vec3 subjectEnd{subject_T_pixel * glm::vec4{pixelEnd, 1.0f}};
    const glm::vec3 worldEnd{world_T_subject * glm::vec4{subjectEnd, 1.0f}};
    axisLengths[axis] = screenDistance(windowViewport, view, worldBase, worldEnd);
  }

  std::sort(axisLengths.begin(), axisLengths.end(), std::greater<float>{});
  const float meanInPlanePx = 0.5f * (axisLengths[0] + axisLengths[1]);
  return meanInPlanePx > 0.0f ? meanInPlanePx : 1.0f;
}

float warpedGridSpacingMillimeters(
  const ImageSettings& settings,
  const Viewport& windowViewport,
  const View& view,
  const Image& image,
  const glm::vec3& worldCrosshairs)
{
  switch (settings.vectorWarpedGridSpacingMode()) {
    case VectorArrowOverlaySpacingMode::Pixels:
      return std::max(settings.vectorWarpedGridPixelSpacing(), 1.0f) /
             screenPixelsPerMillimeter(windowViewport, view, worldCrosshairs);
    case VectorArrowOverlaySpacingMode::Voxels: {
      const float pxPerVoxel = screenPixelsPerVoxel(windowViewport, view, image, worldCrosshairs);
      const float pxPerMm = screenPixelsPerMillimeter(windowViewport, view, worldCrosshairs);
      return std::max(settings.vectorWarpedGridVoxelSpacing(), 0.1f) * pxPerVoxel / std::max(pxPerMm, 1.0e-6f);
    }
    case VectorArrowOverlaySpacingMode::Millimeters:
      return std::max(settings.vectorWarpedGridMillimeterSpacing(), 0.1f);
  }

  return std::max(settings.vectorWarpedGridMillimeterSpacing(), 0.1f);
}

} // namespace rendering::vector_overlay
