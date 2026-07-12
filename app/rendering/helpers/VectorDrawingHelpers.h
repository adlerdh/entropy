#pragma once

#include "viewer/ViewModes.h"

#include "image/ImageSettings.h"

#include <array>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/mat4x4.hpp>

#include <optional>

class Viewport;

namespace entropy::rendering::vector_drawing
{

struct ArrowGeometry
{
  glm::vec2 shaftStart{0.0f};
  glm::vec2 shaftEnd{0.0f};
  glm::vec2 headTip{0.0f};
  glm::vec2 headLeft{0.0f};
  glm::vec2 headRight{0.0f};
};

/**
 * @brief Return whether a 2D vector has finite coordinates.
 */
bool isFiniteVec2(const glm::vec2& value);

/**
 * @brief Return whether a position is inside the half-open rectangle [min, min + size).
 */
bool isInsideRect(const glm::vec2& value, const glm::vec2& min, const glm::vec2& size);

/**
 * @brief Compute the shaft and head geometry for a screen-space arrow.
 */
std::optional<ArrowGeometry> computeArrowGeometry(const glm::vec2& start, const glm::vec2& end, float width);

/**
 * @brief Compute checkerboard cell coordinates from a view-clip position and the view aspect ratio.
 */
glm::vec2 checkerCoordForViewClip(const glm::vec2& viewClipPos, float numCheckers, float aspectRatio);

/**
 * @brief Return whether a sample belongs to the fixed-image side of the current comparison display mode.
 */
bool shouldRenderFixedComparisonSample(
  ViewRenderMode renderMode,
  const glm::vec2& viewClipPos,
  const glm::vec2& checkerCoord,
  const glm::vec2& clipCrosshairs,
  const glm::ivec2& quadrants,
  bool showFixedImage,
  float aspectRatio,
  float flashlightRadius,
  bool flashlightMovingOnFixed);

/**
 * @brief Project a world point into miewport coordinates from a precomputed window-clip transform.
 */
glm::vec2
projectWorldToMiewport(const Viewport& windowViewport, const glm::mat4& windowClip_T_world, const glm::vec3& worldPos);

/**
 * @brief Convert a miewport position into view-clip coordinates.
 */
glm::vec2 viewClipFromMiewport(
  const Viewport& windowViewport,
  const glm::mat4& viewClip_T_windowClip,
  const glm::vec2& miewportPos);

/**
 * @brief Return the screen distance between two projected points, or zero if either point is invalid.
 */
float screenDistanceFromMiewportPositions(const glm::vec2& a, const glm::vec2& b);

/**
 * @brief Return the average positive pixel scale from two screen-axis measurements, or one for invalid input.
 */
float meanScreenPixelsPerMillimeter(float rightPx, float upPx);

/**
 * @brief Return the mean of the two largest voxel-axis screen lengths, or one for invalid input.
 */
float meanInPlaneScreenPixelsPerVoxel(std::array<float, 3> axisLengths);

/**
 * @brief Convert vector-arrow spacing settings to screen pixels.
 */
float vectorArrowSpacingPixels(
  VectorArrowOverlaySpacingMode spacingMode,
  float pixelSpacing,
  float voxelSpacing,
  float millimeterSpacing,
  float screenPixelsPerVoxel,
  float screenPixelsPerMillimeter);

} // namespace entropy::rendering::vector_drawing
