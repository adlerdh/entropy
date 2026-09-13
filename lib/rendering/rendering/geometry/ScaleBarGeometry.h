#pragma once

#include "common/Types.h"

#include <glm/vec2.hpp>

#include <optional>
#include <string>

namespace rendering::scale_bar
{
struct Layout
{
  double lengthMm = 0.0;
  float barLengthPx = 0.0f;
  int intervals = 1;
  glm::vec2 start{0.0f, 0.0f};
  glm::vec2 end{0.0f, 0.0f};
  glm::vec2 tickAxis{0.0f, 0.0f};
  glm::vec2 labelPos{0.0f, 0.0f};
  float fontSizePixels = 0.0f;
  std::string label;
};

/**
 * @brief Return a clean physical scale-bar length within the drawable physical bounds.
 */
double computeNiceScaleBarLengthMm(double targetLengthMm, double minLengthMm, double maxLengthMm);

/**
 * @brief Format a physical scale-bar length using an appropriate metric unit.
 */
std::string formatScaleBarLength(double lengthMm, int precision = 2);

/**
 * @brief Return the number of ruler intervals for a scale bar of the given pixel length.
 */
int computeScaleBarIntervals(float barLengthPx, ScaleBarTicks ticks);

/**
 * @brief Return the drawing direction for the selected scale-bar orientation.
 */
glm::vec2 direction(ScaleBarOrientation orientation);

/**
 * @brief Return the tick direction for the selected scale-bar orientation.
 */
glm::vec2 tickDirection(ScaleBarOrientation orientation);

bool isBottom(ScaleBarPosition position);
bool isRight(ScaleBarPosition position);
bool isLeft(ScaleBarPosition position);
bool isTop(ScaleBarPosition position);

/**
 * @brief Compute scale-bar geometry, or nullopt when the view cannot contain a readable scale bar.
 */
std::optional<Layout> computeLayout(
  const FrameBounds& miewportViewBounds,
  const glm::vec2& worldMmPerPixel,
  ScaleBarPosition position,
  ScaleBarOrientation orientation,
  ScaleBarTicks ticks,
  float targetFraction,
  float marginPx,
  int lengthPrecision = 2);
} // namespace rendering::scale_bar
