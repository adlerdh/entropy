#include "rendering/helpers/ImageDrawingHelpers.h"

#include <glm/geometric.hpp>
#include <glm/vec4.hpp>

#include <array>
#include <algorithm>
#include <cmath>

namespace rendering::image_drawing
{
namespace
{

glm::vec2 windowNdc_T_window(const Viewport& windowViewport, const glm::vec2& windowPixelPos)
{
  return glm::vec2(
    2.0f * (windowPixelPos.x - windowViewport.left()) / windowViewport.width() - 1.0f,
    2.0f * (windowPixelPos.y - windowViewport.bottom()) / windowViewport.height() - 1.0f);
}

} // namespace

glm::vec3 computeTextureSamplingDirectionForViewAxis(
  const glm::mat4& pixel_T_clip,
  const glm::vec3& invPixelDimensions,
  Directions::View axis)
{
  static const glm::vec4 clipOrigin{0.0f, 0.0f, -1.0f, 1.0};
  const glm::vec4 clipPos = clipOrigin + glm::vec4{Directions::get(axis), 0.0f};

  const glm::vec4 pixelOrigin = pixel_T_clip * clipOrigin;
  const glm::vec4 pixelPos = pixel_T_clip * clipPos;

  const glm::vec3 pixelDir = glm::normalize(pixelPos / pixelPos.w - pixelOrigin / pixelOrigin.w);

  return glm::dot(glm::abs(pixelDir), invPixelDimensions) * pixelDir;
}

glm::vec3 computeTextureSamplingDirectionForViewPixelOffset(
  const glm::mat4& texture_T_viewClip,
  const Viewport& windowViewport,
  const glm::mat4& viewClip_T_windowClip,
  const glm::vec2& windowPixelDirection)
{
  static const glm::vec2 winPixelOrigin(0.0f, 0.0f);

  const glm::vec4 winNdcOrigin{windowNdc_T_window(windowViewport, winPixelOrigin), -1.0f, 1.0f};
  const glm::vec4 winNdcPos{windowNdc_T_window(windowViewport, windowPixelDirection), -1.0f, 1.0f};

  glm::vec4 viewNdcOrigin = viewClip_T_windowClip * winNdcOrigin;
  viewNdcOrigin /= viewNdcOrigin.w;
  glm::vec4 viewNdcPos = viewClip_T_windowClip * winNdcPos;
  viewNdcPos /= viewNdcPos.w;

  glm::vec4 texOrigin = texture_T_viewClip * viewNdcOrigin;
  texOrigin /= texOrigin.w;
  glm::vec4 texPos = texture_T_viewClip * viewNdcPos;
  texPos /= texPos.w;

  return glm::vec3{texPos - texOrigin};
}

glm::vec3 computeTextureSamplingDirectionForImageVoxelOffset(
  const glm::mat4& voxel_T_viewClip,
  const Viewport& windowViewport,
  const glm::mat4& viewClip_T_windowClip,
  const glm::vec3& invPixelDimensions,
  const glm::vec2& windowPixelDirection)
{
  static const glm::vec2 winPixelOrigin(0.0f, 0.0f);

  const glm::vec4 winNdcOrigin{windowNdc_T_window(windowViewport, winPixelOrigin), -1.0f, 1.0f};
  const glm::vec4 winNdcPos{windowNdc_T_window(windowViewport, windowPixelDirection), -1.0f, 1.0f};

  glm::vec4 viewNdcOrigin = viewClip_T_windowClip * winNdcOrigin;
  viewNdcOrigin /= viewNdcOrigin.w;
  glm::vec4 viewNdcPos = viewClip_T_windowClip * winNdcPos;
  viewNdcPos /= viewNdcPos.w;

  glm::vec4 voxelOrigin = voxel_T_viewClip * viewNdcOrigin;
  voxelOrigin /= voxelOrigin.w;
  glm::vec4 voxelPos = voxel_T_viewClip * viewNdcPos;
  voxelPos /= voxelPos.w;

  const glm::vec3 voxelDir = glm::normalize(voxelPos - voxelOrigin);
  const glm::vec3 texDir = glm::dot(glm::abs(voxelDir), invPixelDimensions) * voxelDir;

  return texDir;
}

MipSamplingParams
computeMipSamplingParams(float mmPerSample, const glm::uvec3& imageDimensions, float slabThicknessMm, bool useMaxExtent)
{
  MipSamplingParams params;

  if (mmPerSample <= 0.0f) {
    return params;
  }

  params.samplingDistanceCm = mmPerSample / 10.0f;

  if (useMaxExtent) {
    params.halfNumSamples = static_cast<int>(std::ceil(glm::length(glm::vec3{imageDimensions})));
    return params;
  }

  params.halfNumSamples = std::max(0, static_cast<int>(std::floor(0.5f * slabThicknessMm / mmPerSample)));
  return params;
}

float maxScreenPixelsPerVoxelAxis(
  const glm::mat4& viewClip_T_voxel,
  const glm::mat4& windowClip_T_viewClip,
  const Viewport& windowViewport)
{
  const glm::mat4 windowClip_T_voxel = windowClip_T_viewClip * viewClip_T_voxel;

  auto project = [&](const glm::vec3& voxelPos) {
    glm::vec4 windowClip = windowClip_T_voxel * glm::vec4{voxelPos, 1.0f};
    windowClip /= windowClip.w;
    return glm::vec2{
      0.5f * (windowClip.x + 1.0f) * windowViewport.width(),
      0.5f * (windowClip.y + 1.0f) * windowViewport.height()};
  };

  const glm::vec2 origin = project(glm::vec3{0.0f});
  const std::array axisLengths{
    glm::length(project(glm::vec3{1.0f, 0.0f, 0.0f}) - origin),
    glm::length(project(glm::vec3{0.0f, 1.0f, 0.0f}) - origin),
    glm::length(project(glm::vec3{0.0f, 0.0f, 1.0f}) - origin)};

  const float maxLength = *std::max_element(axisLengths.begin(), axisLengths.end());
  return std::isfinite(maxLength) && maxLength > 0.0f ? maxLength : 1.0f;
}

bool automaticFloatingPointInterpolationEnabled(float screenPixelsPerVoxel, float turnOnThresholdPx)
{
  return std::isfinite(screenPixelsPerVoxel) && screenPixelsPerVoxel >= turnOnThresholdPx;
}

} // namespace rendering::image_drawing
