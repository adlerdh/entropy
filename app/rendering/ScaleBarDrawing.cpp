#include "rendering/ScaleBarDrawing.h"

#include "rendering/ScaleBarGeometry.h"

#include "common/Viewport.h"

#include "logic/camera/CameraHelpers.h"

#include "windowing/View.h"

#include <glm/glm.hpp>

#include <nanovg.h>

#include <string>

namespace
{
static const std::string ROBOTO_LIGHT("robotoLight");
constexpr float kShadowBlur = 2.0f;

void drawRuler(
  NVGcontext* nvg,
  const entropy::rendering::scale_bar::Layout& layout,
  const glm::vec4& rulerColor,
  float strokeWidth)
{
  nvgStrokeWidth(nvg, strokeWidth);
  nvgStrokeColor(nvg, nvgRGBAf(rulerColor.r, rulerColor.g, rulerColor.b, rulerColor.a));
  nvgLineCap(nvg, NVG_BUTT);

  nvgBeginPath(nvg);
  nvgMoveTo(nvg, layout.start.x, layout.start.y);
  nvgLineTo(nvg, layout.end.x, layout.end.y);

  for (int i = 0; i <= layout.intervals; ++i) {
    constexpr float tickLengthPx = 9.0f;
    const float t = static_cast<float>(i) / static_cast<float>(layout.intervals);
    const glm::vec2 p = layout.start + t * (layout.end - layout.start);
    const glm::vec2 tickStart = p - 0.5f * tickLengthPx * layout.tickAxis;
    const glm::vec2 tickEnd = p + 0.5f * tickLengthPx * layout.tickAxis;
    nvgMoveTo(nvg, tickStart.x, tickStart.y);
    nvgLineTo(nvg, tickEnd.x, tickEnd.y);
  }

  nvgStroke(nvg);
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
  float marginPx,
  int lengthPrecision)
{
  const glm::vec2 worldMmPerPixel = helper::worldPixelSize(windowVP, view.camera(), view.viewClip_T_windowClip());
  const auto layout = entropy::rendering::scale_bar::computeLayout(
    miewportViewBounds,
    worldMmPerPixel,
    position,
    orientation,
    ticks,
    targetFraction,
    marginPx,
    lengthPrecision);
  if (!layout) {
    return;
  }

  nvgScissor(
    nvg,
    miewportViewBounds.viewport[0],
    miewportViewBounds.viewport[1],
    miewportViewBounds.viewport[2],
    miewportViewBounds.viewport[3]);

  drawRuler(nvg, *layout, glm::vec4{0.0f, 0.0f, 0.0f, color.a}, 4.0f);
  drawRuler(nvg, *layout, color, 2.0f);

  nvgFontSize(nvg, layout->fontSizePixels);
  nvgFontFace(nvg, ROBOTO_LIGHT.c_str());
  nvgTextAlign(nvg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);

  nvgFontBlur(nvg, kShadowBlur);
  nvgFillColor(nvg, nvgRGBAf(0.0f, 0.0f, 0.0f, color.a));
  nvgText(nvg, layout->labelPos.x, layout->labelPos.y, layout->label.c_str(), nullptr);

  nvgFontBlur(nvg, 0.0f);
  nvgFillColor(nvg, nvgRGBAf(color.r, color.g, color.b, color.a));
  nvgText(nvg, layout->labelPos.x, layout->labelPos.y, layout->label.c_str(), nullptr);

  nvgResetScissor(nvg);
}
