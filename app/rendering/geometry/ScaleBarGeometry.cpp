#include "rendering/geometry/ScaleBarGeometry.h"

#include <glm/glm.hpp>

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <initializer_list>
#include <sstream>

namespace rendering::scale_bar
{
namespace
{
constexpr float kTickLengthPx = 9.0f;
constexpr float kMinBarLengthPx = 28.0f;

float paddingPx(float marginPx)
{
  return std::max(marginPx, 12.0f);
}

std::string trimTrailingDecimalZeros(std::string value)
{
  const auto decimalPos = value.find('.');
  if (std::string::npos == decimalPos) {
    return value;
  }

  while (!value.empty() && value.back() == '0') {
    value.pop_back();
  }
  if (!value.empty() && value.back() == '.') {
    value.pop_back();
  }
  return value;
}
} // namespace

double computeNiceScaleBarLengthMm(double targetLengthMm, double minLengthMm, double maxLengthMm)
{
  if (
    targetLengthMm <= 0.0 || minLengthMm <= 0.0 || maxLengthMm <= 0.0 || !std::isfinite(targetLengthMm) ||
    !std::isfinite(minLengthMm) || !std::isfinite(maxLengthMm) || minLengthMm > maxLengthMm)
  {
    return 0.0;
  }

  const double minPower = std::floor(std::log10(minLengthMm)) - 1.0;
  const double maxPower = std::ceil(std::log10(maxLengthMm)) + 1.0;
  double bestUnderTarget = 0.0;
  double smallestFit = 0.0;

  for (double power = minPower; power <= maxPower; power += 1.0) {
    const double magnitude = std::pow(10.0, power);
    for (const double multiplier : {1.0, 2.0, 5.0, 10.0}) {
      const double candidate = multiplier * magnitude;
      if (candidate < minLengthMm || candidate > maxLengthMm) {
        continue;
      }

      if (candidate <= targetLengthMm) {
        bestUnderTarget = std::max(bestUnderTarget, candidate);
      }
      if (0.0 == smallestFit || candidate < smallestFit) {
        smallestFit = candidate;
      }
    }
  }

  return bestUnderTarget > 0.0 ? bestUnderTarget : smallestFit;
}

std::string formatScaleBarLength(double lengthMm, int precision)
{
  double value = lengthMm;
  const char* unit = "mm";

  if (lengthMm >= 1.0e6) {
    value = lengthMm / 1.0e6;
    unit = "km";
  }
  else if (lengthMm >= 1000.0) {
    value = lengthMm / 1000.0;
    unit = "m";
  }
  else if (lengthMm >= 10.0) {
    value = lengthMm / 10.0;
    unit = "cm";
  }
  else if (lengthMm >= 1.0) {
    value = lengthMm;
    unit = "mm";
  }
  else if (lengthMm >= 1.0e-3) {
    value = lengthMm * 1000.0;
    unit = "µm";
  }
  else {
    value = lengthMm * 1.0e6;
    unit = "nm";
  }

  std::ostringstream out;
  if (std::abs(value - std::round(value)) < 1.0e-6) {
    out << static_cast<int>(std::round(value));
  }
  else {
    out << std::fixed << std::setprecision(std::max(0, precision)) << value;
  }
  return trimTrailingDecimalZeros(out.str()) + " " + unit;
}

int computeScaleBarIntervals(float barLengthPx, ScaleBarTicks ticks)
{
  if (ScaleBarTicks::Automatic != ticks) {
    return 1;
  }

  static constexpr float kMinTickSpacingPx = 24.0f;
  for (const int intervals : {5, 4, 2}) {
    if (barLengthPx / static_cast<float>(intervals) >= kMinTickSpacingPx) {
      return intervals;
    }
  }
  return 1;
}

glm::vec2 direction(ScaleBarOrientation orientation)
{
  return (ScaleBarOrientation::Horizontal == orientation) ? glm::vec2{1.0f, 0.0f} : glm::vec2{0.0f, -1.0f};
}

glm::vec2 tickDirection(ScaleBarOrientation orientation)
{
  return (ScaleBarOrientation::Horizontal == orientation) ? glm::vec2{0.0f, 1.0f} : glm::vec2{1.0f, 0.0f};
}

bool isBottom(ScaleBarPosition position)
{
  return ScaleBarPosition::BottomRight == position || ScaleBarPosition::BottomLeft == position ||
         ScaleBarPosition::Bottom == position;
}

bool isRight(ScaleBarPosition position)
{
  return ScaleBarPosition::BottomRight == position || ScaleBarPosition::TopRight == position ||
         ScaleBarPosition::Right == position;
}

bool isLeft(ScaleBarPosition position)
{
  return ScaleBarPosition::BottomLeft == position || ScaleBarPosition::TopLeft == position ||
         ScaleBarPosition::Left == position;
}

bool isTop(ScaleBarPosition position)
{
  return ScaleBarPosition::TopRight == position || ScaleBarPosition::TopLeft == position ||
         ScaleBarPosition::Top == position;
}

std::optional<Layout> computeLayout(
  const FrameBounds& miewportViewBounds,
  const glm::vec2& worldMmPerPixel,
  ScaleBarPosition position,
  ScaleBarOrientation orientation,
  ScaleBarTicks ticks,
  float targetFraction,
  float marginPx,
  int lengthPrecision)
{
  const float padding = paddingPx(marginPx);
  const glm::vec2 miewportMinCorner(miewportViewBounds.bounds.xoffset, miewportViewBounds.bounds.yoffset);
  const glm::vec2 miewportSize(miewportViewBounds.bounds.width, miewportViewBounds.bounds.height);
  const glm::vec2 miewportMaxCorner = miewportMinCorner + miewportSize;

  const float viewLengthPx = (ScaleBarOrientation::Horizontal == orientation) ? miewportSize.x : miewportSize.y;
  const float usableLengthPx = std::max(viewLengthPx - 2.0f * padding, 0.0f);
  const float targetLengthPx = glm::clamp(targetFraction, 0.05f, 1.0f) * usableLengthPx;
  if (usableLengthPx < kMinBarLengthPx || targetLengthPx <= 0.0f) {
    return std::nullopt;
  }

  const float mmPerPixel = (ScaleBarOrientation::Horizontal == orientation) ? worldMmPerPixel.x : worldMmPerPixel.y;
  if (mmPerPixel <= 0.0f || !std::isfinite(mmPerPixel)) {
    return std::nullopt;
  }

  const double minLengthMm = static_cast<double>(kMinBarLengthPx) * mmPerPixel;
  const double maxLengthMm = static_cast<double>(usableLengthPx) * mmPerPixel;
  const double targetLengthMm = static_cast<double>(targetLengthPx) * mmPerPixel;
  const double lengthMm = computeNiceScaleBarLengthMm(targetLengthMm, minLengthMm, maxLengthMm);
  if (lengthMm <= 0.0) {
    return std::nullopt;
  }

  const float barLengthPx = static_cast<float>(lengthMm / mmPerPixel);
  if (barLengthPx < kMinBarLengthPx || barLengthPx > usableLengthPx) {
    return std::nullopt;
  }

  Layout layout;
  layout.lengthMm = lengthMm;
  layout.barLengthPx = barLengthPx;
  layout.fontSizePixels = glm::clamp(0.045f * std::min(miewportSize.x, miewportSize.y), 10.0f, 16.0f);
  layout.tickAxis = tickDirection(orientation);

  if (ScaleBarOrientation::Horizontal == orientation) {
    if (isRight(position)) {
      layout.start.x = miewportMaxCorner.x - padding - barLengthPx;
    }
    else if (isLeft(position)) {
      layout.start.x = miewportMinCorner.x + padding;
    }
    else {
      layout.start.x = miewportMinCorner.x + 0.5f * (miewportSize.x - barLengthPx);
    }

    if (isTop(position)) {
      layout.start.y = miewportMinCorner.y + padding + 0.5f * kTickLengthPx;
    }
    else if (isBottom(position)) {
      layout.start.y = miewportMaxCorner.y - padding - 0.5f * kTickLengthPx;
    }
    else {
      layout.start.y = miewportMinCorner.y + 0.5f * miewportSize.y;
    }
  }
  else {
    if (isRight(position)) {
      layout.start.x = miewportMaxCorner.x - padding - 0.5f * kTickLengthPx;
    }
    else if (isLeft(position)) {
      layout.start.x = miewportMinCorner.x + padding + 0.5f * kTickLengthPx;
    }
    else {
      layout.start.x = miewportMinCorner.x + 0.5f * miewportSize.x;
    }

    if (isBottom(position)) {
      layout.start.y = miewportMaxCorner.y - padding;
    }
    else if (isTop(position)) {
      layout.start.y = miewportMinCorner.y + padding + barLengthPx;
    }
    else {
      layout.start.y = miewportMinCorner.y + 0.5f * (miewportSize.y + barLengthPx);
    }
  }

  const float labelGap = 0.35f * layout.fontSizePixels;
  const float labelClearance = 0.65f * layout.fontSizePixels + labelGap;

  layout.end = layout.start + direction(orientation) * barLengthPx;
  layout.intervals = computeScaleBarIntervals(barLengthPx, ticks);
  layout.label = formatScaleBarLength(lengthMm, lengthPrecision);
  layout.labelPos = 0.5f * (layout.start + layout.end);
  if (ScaleBarOrientation::Horizontal == orientation) {
    layout.labelPos.y += isTop(position) ? labelClearance : -labelClearance;
  }
  else {
    layout.labelPos.x +=
      isRight(position) ? -(labelGap + 1.5f * layout.fontSizePixels) : (labelGap + 1.5f * layout.fontSizePixels);
  }

  return layout;
}
} // namespace rendering::scale_bar
