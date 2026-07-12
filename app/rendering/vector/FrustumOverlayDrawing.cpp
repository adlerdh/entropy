#include "rendering/vector/FrustumOverlayDrawing.h"

#include "rendering/helpers/VectorDrawingHelpers.h"

#include "common/Viewport.h"

#include "logic/camera/CameraFrustumSlice.h"
#include "logic/camera/CameraHelpers.h"

#include "windowing/View.h"

#include <glm/glm.hpp>

#include <nanovg.h>

namespace
{

namespace vector_drawing = entropy::rendering::vector_drawing;

NVGcolor nvgColor(const glm::vec4& color)
{
  return nvgRGBAf(color.r, color.g, color.b, color.a);
}

} // namespace

void drawThreeDCameraFrustumOverlay(
  NVGcontext* nvg,
  const FrameBounds& miewportViewBounds,
  const Viewport& windowViewport,
  const View& view,
  const camera3d::FrustumSliceOverlay& overlay,
  const glm::vec4& color)
{
  static constexpr float k_eyeRadiusPx = 4.0f;
  static constexpr float k_shadowAlphaScale = 0.65f;
  const NVGcolor lineColor = nvgColor(color);
  const NVGcolor shadowColor = nvgRGBAf(0.0f, 0.0f, 0.0f, k_shadowAlphaScale * color.a);

  nvgScissor(
    nvg,
    miewportViewBounds.viewport[0],
    miewportViewBounds.viewport[1],
    miewportViewBounds.viewport[2],
    miewportViewBounds.viewport[3]);

  nvgLineCap(nvg, NVG_BUTT);
  nvgLineJoin(nvg, NVG_MITER);
  const glm::mat4 windowClip_T_world = view.windowClip_T_viewClip() * helper::clip_T_world(view.camera());

  auto drawSegments = [&](const NVGcolor& strokeColor, float strokeWidth) {
    nvgStrokeColor(nvg, strokeColor);
    nvgStrokeWidth(nvg, strokeWidth);
    nvgBeginPath(nvg);
    for (const auto& segment : overlay.segments) {
      const glm::vec2 a = vector_drawing::projectWorldToMiewport(windowViewport, windowClip_T_world, segment.a);
      const glm::vec2 b = vector_drawing::projectWorldToMiewport(windowViewport, windowClip_T_world, segment.b);
      if (!vector_drawing::isFiniteVec2(a) || !vector_drawing::isFiniteVec2(b)) {
        continue;
      }
      nvgMoveTo(nvg, a.x, a.y);
      nvgLineTo(nvg, b.x, b.y);
    }
    nvgStroke(nvg);
  };

  drawSegments(shadowColor, 4.0f);
  drawSegments(lineColor, 2.0f);

  const glm::vec2 eye = vector_drawing::projectWorldToMiewport(windowViewport, windowClip_T_world, overlay.eye);
  if (vector_drawing::isFiniteVec2(eye)) {
    nvgBeginPath(nvg);
    nvgCircle(nvg, eye.x, eye.y, k_eyeRadiusPx + 2.0f);
    nvgFillColor(nvg, shadowColor);
    nvgFill(nvg);

    nvgBeginPath(nvg);
    nvgCircle(nvg, eye.x, eye.y, k_eyeRadiusPx);
    nvgFillColor(nvg, lineColor);
    nvgFill(nvg);
  }

  nvgResetScissor(nvg);
}
