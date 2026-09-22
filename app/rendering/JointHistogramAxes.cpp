#include "rendering/JointHistogramRenderer.h"

#include "common/Viewport.h"
#include "rendering/vector/VectorDrawing.h"

#include <nanovg.h>

#include <algorithm>
#include <format>
#include <string>

namespace rendering
{

void JointHistogramRenderer::drawAxes(
  NVGcontext* nvg,
  const joint_histogram::Plot& plot,
  const Viewport& windowViewport,
  const std::array<std::string, 2>& imageNames,
  const std::array<std::pair<double, double>, 2>& ranges,
  const joint_histogram::Navigation& navigation,
  int majorTicks,
  int minorTicks)
{
  if (!nvg || plot.size <= 0.0f) {
    return;
  }
  majorTicks = std::clamp(majorTicks, 2, 12);
  minorTicks = std::clamp(minorTicks, 0, 9);
  const float left = plot.left;
  const float right = plot.left + plot.size;
  const float top = plot.top;
  const float bottom = top + plot.size;

  startNvgFrame(nvg, windowViewport);
  nvgStrokeColor(nvg, nvgRGBA(220, 225, 230, 255));
  nvgStrokeWidth(nvg, 1.5f);
  nvgBeginPath(nvg);
  nvgMoveTo(nvg, left, top);
  nvgLineTo(nvg, left, bottom);
  nvgLineTo(nvg, right, bottom);
  nvgStroke(nvg);

  nvgFontFace(nvg, "robotoLight");
  nvgFontSize(nvg, 12.0f);
  nvgFillColor(nvg, nvgRGBA(220, 225, 230, 255));
  for (int axis = 0; axis < 2; ++axis) {
    for (int tick = 0; tick < majorTicks; ++tick) {
      const float fraction = static_cast<float>(tick) / static_cast<float>(majorTicks - 1);
      const float x = left + plot.size * fraction;
      const float y = bottom - plot.size * fraction;
      nvgBeginPath(nvg);
      if (axis == 0) {
        nvgMoveTo(nvg, x, bottom);
        nvgLineTo(nvg, x, bottom + 7.0f);
      }
      else {
        nvgMoveTo(nvg, left - 7.0f, y);
        nvgLineTo(nvg, left, y);
      }
      nvgStroke(nvg);

      const double normalizedValue = navigation.visibleMinimum()[axis] +
                                     fraction * (navigation.visibleMaximum()[axis] - navigation.visibleMinimum()[axis]);
      const double value = ranges[axis].first + normalizedValue * (ranges[axis].second - ranges[axis].first);
      const std::string label = std::format("{:.4g}", value);
      if (axis == 0) {
        nvgTextAlign(nvg, NVG_ALIGN_CENTER | NVG_ALIGN_TOP);
        nvgText(nvg, x, bottom + 10.0f, label.c_str(), nullptr);
      }
      else {
        nvgTextAlign(nvg, NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE);
        nvgText(nvg, left - 10.0f, y, label.c_str(), nullptr);
      }
      if (tick == majorTicks - 1) {
        continue;
      }
      for (int minor = 1; minor <= minorTicks; ++minor) {
        const float t = (static_cast<float>(tick) + static_cast<float>(minor) / (minorTicks + 1)) /
                        static_cast<float>(majorTicks - 1);
        nvgBeginPath(nvg);
        if (axis == 0) {
          const float px = left + plot.size * t;
          nvgMoveTo(nvg, px, bottom);
          nvgLineTo(nvg, px, bottom + 4.0f);
        }
        else {
          const float py = bottom - plot.size * t;
          nvgMoveTo(nvg, left - 4.0f, py);
          nvgLineTo(nvg, left, py);
        }
        nvgStroke(nvg);
      }
    }
  }

  nvgFontSize(nvg, 14.0f);
  nvgTextAlign(nvg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
  const std::string fixedLabel = "Fixed: " + imageNames[0];
  const std::string movingLabel = "Moving: " + imageNames[1];
  nvgText(nvg, 0.5f * (left + right), bottom + 49.0f, fixedLabel.c_str(), nullptr);
  nvgSave(nvg);
  nvgTranslate(nvg, left - 60.0f, 0.5f * (top + bottom));
  nvgRotate(nvg, -0.5f * NVG_PI);
  nvgText(nvg, 0.0f, 0.0f, movingLabel.c_str(), nullptr);
  nvgRestore(nvg);
  endNvgFrame(nvg);
}

} // namespace rendering
