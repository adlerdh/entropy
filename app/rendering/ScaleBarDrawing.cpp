#include "rendering/ScaleBarDrawing.h"

#include "common/Viewport.h"

#include "logic/camera/CameraHelpers.h"

#include "windowing/View.h"

#include <glm/glm.hpp>

#include <nanovg.h>

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <string>

namespace
{
static const std::string ROBOTO_LIGHT("robotoLight");

double computeNiceScaleBarLengthMm(double targetLengthMm)
{
  if (targetLengthMm <= 0.0 || !std::isfinite(targetLengthMm)) {
    return 0.0;
  }

  const double magnitude = std::pow(10.0, std::floor(std::log10(targetLengthMm)));
  for (const double multiplier : {10.0, 5.0, 2.0, 1.0}) {
    const double candidate = multiplier * magnitude;
    if (candidate <= targetLengthMm) {
      return candidate;
    }
  }
  return magnitude;
}

std::string formatScaleBarLength(double lengthMm)
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
    out << std::setprecision(2) << value;
  }
  out << " " << unit;
  return out.str();
}

int computeScaleBarIntervals(float barLengthPx, ScaleBarTicks ticks)
{
  if (ScaleBarTicks::Automatic != ticks) {
    return 1;
  }

  static constexpr float sk_minTickSpacingPx = 24.0f;
  for (const int intervals : {5, 4, 2}) {
    if (barLengthPx / static_cast<float>(intervals) >= sk_minTickSpacingPx) {
      return intervals;
    }
  }
  return 1;
}

glm::vec2 scaleBarDirection(ScaleBarOrientation orientation)
{
  return (ScaleBarOrientation::Horizontal == orientation) ? glm::vec2{1.0f, 0.0f} : glm::vec2{0.0f, -1.0f};
}

glm::vec2 scaleBarTickDirection(ScaleBarOrientation orientation)
{
  return (ScaleBarOrientation::Horizontal == orientation) ? glm::vec2{0.0f, 1.0f} : glm::vec2{1.0f, 0.0f};
}

bool scaleBarIsBottom(ScaleBarPosition position)
{
  return ScaleBarPosition::BottomRight == position || ScaleBarPosition::BottomLeft == position ||
         ScaleBarPosition::Bottom == position;
}

bool scaleBarIsRight(ScaleBarPosition position)
{
  return ScaleBarPosition::BottomRight == position || ScaleBarPosition::TopRight == position ||
         ScaleBarPosition::Right == position;
}

bool scaleBarIsLeft(ScaleBarPosition position)
{
  return ScaleBarPosition::BottomLeft == position || ScaleBarPosition::TopLeft == position ||
         ScaleBarPosition::Left == position;
}

bool scaleBarIsTop(ScaleBarPosition position)
{
  return ScaleBarPosition::TopRight == position || ScaleBarPosition::TopLeft == position ||
         ScaleBarPosition::Top == position;
}
} // namespace

void drawScaleBar(
  NVGcontext* nvg,
  const FrameBounds& miewportViewBounds,
  const Viewport& windowVP,
  const View& view,
  const glm::vec4& color,
  ScaleBarPosition position,
  ScaleBarOrientation orientation,
  ScaleBarTicks ticks,
  float targetFraction,
  float marginPx)
{
  static constexpr float sk_tickLength = 9.0f;
  static constexpr float sk_minBarLengthPx = 28.0f;
  static constexpr float sk_shadowBlur = 2.0f;
  const float padding = std::max(marginPx, 12.0f);

  const glm::vec2 miewportMinCorner(miewportViewBounds.bounds.xoffset, miewportViewBounds.bounds.yoffset);
  const glm::vec2 miewportSize(miewportViewBounds.bounds.width, miewportViewBounds.bounds.height);
  const glm::vec2 miewportMaxCorner = miewportMinCorner + miewportSize;

  const float viewLengthPx = (ScaleBarOrientation::Horizontal == orientation) ? miewportSize.x : miewportSize.y;
  const float usableLengthPx = std::max(viewLengthPx - 2.0f * padding, 0.0f);
  const float targetLengthPx = glm::clamp(targetFraction, 0.05f, 1.0f) * usableLengthPx;
  if (usableLengthPx < sk_minBarLengthPx || targetLengthPx <= 0.0f) {
    return;
  }

  const glm::vec2 worldMmPerPixel = helper::worldPixelSize(windowVP, view.camera(), view.viewClip_T_windowClip());
  const float mmPerPixel = (ScaleBarOrientation::Horizontal == orientation) ? worldMmPerPixel.x : worldMmPerPixel.y;
  if (mmPerPixel <= 0.0f || !std::isfinite(mmPerPixel)) {
    return;
  }

  const double lengthMm = computeNiceScaleBarLengthMm(static_cast<double>(targetLengthPx) * mmPerPixel);
  if (lengthMm <= 0.0) {
    return;
  }

  const float barLengthPx = static_cast<float>(lengthMm / mmPerPixel);
  if (barLengthPx < sk_minBarLengthPx || barLengthPx > usableLengthPx) {
    return;
  }

  const float fontSizePixels = glm::clamp(0.045f * std::min(miewportSize.x, miewportSize.y), 10.0f, 16.0f);
  const float labelGap = 0.35f * fontSizePixels;
  const float labelClearance = 0.65f * fontSizePixels + labelGap;

  glm::vec2 start{0.0f};
  const glm::vec2 axis = scaleBarDirection(orientation);
  const glm::vec2 tickAxis = scaleBarTickDirection(orientation);

  if (ScaleBarOrientation::Horizontal == orientation) {
    if (scaleBarIsRight(position)) {
      start.x = miewportMaxCorner.x - padding - barLengthPx;
    }
    else if (scaleBarIsLeft(position)) {
      start.x = miewportMinCorner.x + padding;
    }
    else {
      start.x = miewportMinCorner.x + 0.5f * (miewportSize.x - barLengthPx);
    }

    if (scaleBarIsTop(position)) {
      start.y = miewportMinCorner.y + padding + 0.5f * sk_tickLength;
    }
    else if (scaleBarIsBottom(position)) {
      start.y = miewportMaxCorner.y - padding - 0.5f * sk_tickLength;
    }
    else {
      start.y = miewportMinCorner.y + 0.5f * miewportSize.y;
    }
  }
  else {
    if (scaleBarIsRight(position)) {
      start.x = miewportMaxCorner.x - padding - 0.5f * sk_tickLength;
    }
    else if (scaleBarIsLeft(position)) {
      start.x = miewportMinCorner.x + padding + 0.5f * sk_tickLength;
    }
    else {
      start.x = miewportMinCorner.x + 0.5f * miewportSize.x;
    }

    if (scaleBarIsBottom(position)) {
      start.y = miewportMaxCorner.y - padding;
    }
    else if (scaleBarIsTop(position)) {
      start.y = miewportMinCorner.y + padding + barLengthPx;
    }
    else {
      start.y = miewportMinCorner.y + 0.5f * (miewportSize.y + barLengthPx);
    }
  }

  const glm::vec2 end = start + axis * barLengthPx;
  const int intervals = computeScaleBarIntervals(barLengthPx, ticks);

  nvgScissor(
    nvg,
    miewportViewBounds.viewport[0],
    miewportViewBounds.viewport[1],
    miewportViewBounds.viewport[2],
    miewportViewBounds.viewport[3]);

  auto drawRuler = [&](const glm::vec4& rulerColor, float strokeWidth) {
    nvgStrokeWidth(nvg, strokeWidth);
    nvgStrokeColor(nvg, nvgRGBAf(rulerColor.r, rulerColor.g, rulerColor.b, rulerColor.a));
    nvgLineCap(nvg, NVG_BUTT);

    nvgBeginPath(nvg);
    nvgMoveTo(nvg, start.x, start.y);
    nvgLineTo(nvg, end.x, end.y);

    for (int i = 0; i <= intervals; ++i) {
      const float t = static_cast<float>(i) / static_cast<float>(intervals);
      const glm::vec2 p = start + t * (end - start);
      const glm::vec2 tickStart = p - 0.5f * sk_tickLength * tickAxis;
      const glm::vec2 tickEnd = p + 0.5f * sk_tickLength * tickAxis;
      nvgMoveTo(nvg, tickStart.x, tickStart.y);
      nvgLineTo(nvg, tickEnd.x, tickEnd.y);
    }
    nvgStroke(nvg);
  };

  drawRuler(glm::vec4{0.0f, 0.0f, 0.0f, color.a}, 4.0f);
  drawRuler(color, 2.0f);

  const std::string label = formatScaleBarLength(lengthMm);
  glm::vec2 labelPos{0.5f * (start + end)};
  if (ScaleBarOrientation::Horizontal == orientation) {
    labelPos.y += scaleBarIsTop(position) ? labelClearance : -labelClearance;
  }
  else {
    labelPos.x += scaleBarIsRight(position) ? -(labelGap + 1.5f * fontSizePixels) : (labelGap + 1.5f * fontSizePixels);
  }

  nvgFontSize(nvg, fontSizePixels);
  nvgFontFace(nvg, ROBOTO_LIGHT.c_str());
  nvgTextAlign(nvg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);

  nvgFontBlur(nvg, sk_shadowBlur);
  nvgFillColor(nvg, nvgRGBAf(0.0f, 0.0f, 0.0f, color.a));
  nvgText(nvg, labelPos.x, labelPos.y, label.c_str(), nullptr);

  nvgFontBlur(nvg, 0.0f);
  nvgFillColor(nvg, nvgRGBAf(color.r, color.g, color.b, color.a));
  nvgText(nvg, labelPos.x, labelPos.y, label.c_str(), nullptr);

  nvgResetScissor(nvg);
}
