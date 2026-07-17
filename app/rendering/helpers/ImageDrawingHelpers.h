#pragma once

#include "common/DirectionMaps.h"
#include "common/Viewport.h"

#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

namespace rendering::image_drawing
{

struct MipSamplingParams
{
  int halfNumSamples = 0;
  float samplingDistanceCm = 0.0f;
};

/**
 * @brief Compute the texture-space sample direction corresponding to a canonical view axis.
 */
glm::vec3 computeTextureSamplingDirectionForViewAxis(
  const glm::mat4& pixel_T_clip,
  const glm::vec3& invPixelDimensions,
  Directions::View axis);

/**
 * @brief Compute the texture-space offset corresponding to a one-pixel step in the window.
 */
glm::vec3 computeTextureSamplingDirectionForViewPixelOffset(
  const glm::mat4& texture_T_viewClip,
  const Viewport& windowViewport,
  const glm::mat4& viewClip_T_windowClip,
  const glm::vec2& windowPixelDirection);

/**
 * @brief Compute the texture-space offset corresponding to a one-voxel step along a projected view direction.
 */
glm::vec3 computeTextureSamplingDirectionForImageVoxelOffset(
  const glm::mat4& voxel_T_viewClip,
  const Viewport& windowViewport,
  const glm::mat4& viewClip_T_windowClip,
  const glm::vec3& invPixelDimensions,
  const glm::vec2& windowPixelDirection);

/**
 * @brief Compute the slab sample count and sample spacing used by intensity projection modes.
 */
MipSamplingParams computeMipSamplingParams(
  float mmPerSample,
  const glm::uvec3& imageDimensions,
  float slabThicknessMm,
  bool useMaxExtent);

/**
 * @brief Estimate the largest projected screen length of one image voxel axis.
 */
float maxScreenPixelsPerVoxelAxis(
  const glm::mat4& viewClip_T_voxel,
  const glm::mat4& windowClip_T_viewClip,
  const Viewport& windowViewport);

/**
 * @brief Return whether automatic floating-point interpolation should be active for the current magnification.
 */
bool automaticFloatingPointInterpolationEnabled(float screenPixelsPerVoxel, float turnOnThresholdPx);

} // namespace rendering::image_drawing
