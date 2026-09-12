#include "rendering/vector/TransformationGuideDrawing.h"

#include "common/Viewport.h"
#include "logic/camera/CameraHelpers.h"
#include "windowing/View.h"

#include <glm/geometric.hpp>
#include <glm/trigonometric.hpp>
#include <glm/vec2.hpp>
#include <nanovg.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <iomanip>
#include <iterator>
#include <numeric>
#include <sstream>
#include <string>
#include <type_traits>
#include <variant>

namespace rendering::vector_overlay
{
namespace
{
constexpr char kFontFace[] = "robotoLight";
constexpr float kMinimumArrowLength = 4.0f;

NVGcolor nvgColor(const glm::vec4& color, const float opacity)
{
  return nvgRGBAf(color.r, color.g, color.b, std::clamp(color.a * opacity, 0.0f, 1.0f));
}

void strokeLine(NVGcontext* nvg, const glm::vec2& start, const glm::vec2& end, const NVGcolor color, const float width)
{
  nvgBeginPath(nvg);
  nvgMoveTo(nvg, start.x, start.y);
  nvgLineTo(nvg, end.x, end.y);
  nvgStrokeWidth(nvg, width);
  nvgStrokeColor(nvg, color);
  nvgLineCap(nvg, NVG_ROUND);
  nvgStroke(nvg);
}

void drawDashedLine(
  NVGcontext* nvg,
  const glm::vec2& start,
  const glm::vec2& end,
  const NVGcolor color,
  const float width,
  const float scale)
{
  const glm::vec2 delta = end - start;
  const float length = glm::length(delta);
  if (length < kMinimumArrowLength * scale) {
    return;
  }

  const glm::vec2 direction = delta / length;
  const float dashLength = 5.0f * scale;
  const float period = 9.0f * scale;
  const auto dashCount = static_cast<std::size_t>(std::ceil(length / period));
  for (std::size_t dashIndex = 0; dashIndex < dashCount; ++dashIndex) {
    const float offset = static_cast<float>(dashIndex) * period;
    strokeLine(
      nvg,
      start + offset * direction,
      start + std::min(offset + dashLength, length) * direction,
      color,
      width);
  }
}

void drawArrow(
  NVGcontext* nvg,
  const glm::vec2& start,
  const glm::vec2& end,
  const NVGcolor color,
  const float width,
  const float scale)
{
  const glm::vec2 delta = end - start;
  const float length = glm::length(delta);
  if (length < kMinimumArrowLength * scale) {
    return;
  }

  const glm::vec2 direction = delta / length;
  const glm::vec2 normal{-direction.y, direction.x};
  const float headLength = std::min(10.0f * scale, 0.35f * length);
  const float headHalfWidth = 0.5f * headLength;
  const glm::vec2 headBase = end - headLength * direction;

  strokeLine(nvg, start, headBase, color, width);
  nvgBeginPath(nvg);
  nvgMoveTo(nvg, end.x, end.y);
  nvgLineTo(nvg, (headBase + headHalfWidth * normal).x, (headBase + headHalfWidth * normal).y);
  nvgLineTo(nvg, (headBase - headHalfWidth * normal).x, (headBase - headHalfWidth * normal).y);
  nvgClosePath(nvg);
  nvgFillColor(nvg, color);
  nvgFill(nvg);
}

void strokePolyline(
  NVGcontext* nvg,
  const std::vector<glm::vec2>& points,
  const NVGcolor color,
  const float width,
  const bool closed = false)
{
  if (points.size() < 2) {
    return;
  }

  nvgBeginPath(nvg);
  nvgMoveTo(nvg, points.front().x, points.front().y);
  for (std::size_t i = 1; i < points.size(); ++i) {
    nvgLineTo(nvg, points[i].x, points[i].y);
  }
  if (closed) {
    nvgClosePath(nvg);
  }
  nvgStrokeWidth(nvg, width);
  nvgStrokeColor(nvg, color);
  nvgLineCap(nvg, NVG_ROUND);
  nvgLineJoin(nvg, NVG_ROUND);
  nvgStroke(nvg);
}

std::string fixed(const float value, const int precision)
{
  std::ostringstream stream;
  stream << std::fixed << std::setprecision(std::clamp(precision, 0, 9)) << value;
  return stream.str();
}

void drawShadowedText(
  NVGcontext* nvg,
  const glm::vec2& position,
  const std::string& text,
  const NVGcolor color,
  const float shadowOpacity)
{
  nvgFontBlur(nvg, 2.0f);
  nvgFillColor(nvg, nvgRGBAf(0.0f, 0.0f, 0.0f, 0.9f * shadowOpacity));
  nvgText(nvg, position.x, position.y, text.c_str(), nullptr);
  nvgFontBlur(nvg, 0.0f);
  nvgFillColor(nvg, color);
  nvgText(nvg, position.x, position.y, text.c_str(), nullptr);
}

glm::vec2 clampedTextPosition(
  NVGcontext* nvg,
  const FrameBounds& frameBounds,
  const glm::vec2& desired,
  const std::array<std::string, 2>& lines,
  const float totalHeight,
  const float margin)
{
  const float maxWidth =
    std::accumulate(lines.begin(), lines.end(), 0.0f, [nvg](const float maximum, const std::string& line) {
      return std::max(maximum, nvgTextBounds(nvg, 0.0f, 0.0f, line.c_str(), nullptr, nullptr));
    });

  const float left = frameBounds.bounds.xoffset + margin;
  const float right = frameBounds.bounds.xoffset + frameBounds.bounds.width - margin - maxWidth;
  const float top = frameBounds.bounds.yoffset + margin;
  const float bottom = frameBounds.bounds.yoffset + frameBounds.bounds.height - margin - totalHeight;
  return {std::clamp(desired.x, left, std::max(left, right)), std::clamp(desired.y, top, std::max(top, bottom))};
}

glm::vec2 projectWorldPoint(const Viewport& windowViewport, const View& view, const glm::vec3& worldPoint)
{
  return helper::miewport_T_world(windowViewport, view.camera(), view.windowClip_T_viewClip(), worldPoint);
}

std::vector<glm::vec2>
projectWorldPoints(const Viewport& windowViewport, const View& view, const std::vector<glm::vec3>& worldPoints)
{
  std::vector<glm::vec2> projected;
  projected.reserve(worldPoints.size());
  std::ranges::transform(worldPoints, std::back_inserter(projected), [&](const glm::vec3& point) {
    return projectWorldPoint(windowViewport, view, point);
  });
  return projected;
}

void beginGuideDrawing(NVGcontext* nvg, const FrameBounds& frameBounds)
{
  nvgSave(nvg);
  nvgScissor(
    nvg,
    frameBounds.bounds.xoffset,
    frameBounds.bounds.yoffset,
    frameBounds.bounds.width,
    frameBounds.bounds.height);
}

void drawCenterMarker(
  NVGcontext* nvg,
  const glm::vec2& center,
  const NVGcolor shadow,
  const NVGcolor color,
  const float scale)
{
  nvgBeginPath(nvg);
  nvgCircle(nvg, center.x, center.y, 4.25f * scale);
  nvgStrokeWidth(nvg, 3.5f * scale);
  nvgStrokeColor(nvg, shadow);
  nvgStroke(nvg);

  nvgBeginPath(nvg);
  nvgCircle(nvg, center.x, center.y, 3.5f * scale);
  nvgStrokeWidth(nvg, 1.5f * scale);
  nvgStrokeColor(nvg, color);
  nvgStroke(nvg);
}

void drawLabels(
  NVGcontext* nvg,
  const FrameBounds& frameBounds,
  const glm::vec2& desiredPosition,
  const std::array<std::string, 2>& labels,
  const NVGcolor color,
  const float shadowOpacity,
  const float scale)
{
  const float fontSize = 14.0f * scale;
  const float lineHeight = 17.0f * scale;
  const float margin = 8.0f * scale;
  nvgFontFace(nvg, kFontFace);
  nvgFontSize(nvg, fontSize);
  nvgTextAlign(nvg, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
  const float totalHeight = labels[1].empty() ? lineHeight : 2.0f * lineHeight;
  const glm::vec2 position = clampedTextPosition(nvg, frameBounds, desiredPosition, labels, totalHeight, margin);
  drawShadowedText(nvg, position, labels[0], color, shadowOpacity);
  if (!labels[1].empty()) {
    drawShadowedText(nvg, position + glm::vec2{0.0f, lineHeight}, labels[1], color, shadowOpacity);
  }
}
} // namespace

void drawTranslationGuide(
  NVGcontext* nvg,
  const FrameBounds& frameBounds,
  const Viewport& windowViewport,
  const View& view,
  const interaction::TranslationGuide& guide,
  const glm::vec4& color,
  const float uiScale,
  const int precision,
  const bool showParameters)
{
  const float opacity = guide.presentation.opacity;
  if (!nvg || opacity <= 0.0f || frameBounds.bounds.width <= 0.0f || frameBounds.bounds.height <= 0.0f) {
    return;
  }

  const float scale = std::clamp(uiScale, 0.5f, 4.0f);
  const float shadowOpacity = opacity * std::clamp(color.a, 0.0f, 1.0f);
  const glm::vec3 endWorld = guide.startWorld + guide.displacementWorld;
  const glm::vec2 start = projectWorldPoint(windowViewport, view, guide.startWorld);
  const glm::vec2 end = projectWorldPoint(windowViewport, view, endWorld);
  const auto componentWorldEnds = interaction::translationComponentEndpoints(guide);

  const glm::vec4 shadowColor{0.0f, 0.0f, 0.0f, 0.82f};
  constexpr std::array<glm::vec4, 3> axisColors{
    glm::vec4{1.0f, 0.30f, 0.25f, 0.86f},
    glm::vec4{0.30f, 0.90f, 0.35f, 0.86f},
    glm::vec4{0.30f, 0.62f, 1.0f, 0.86f}};

  beginGuideDrawing(nvg, frameBounds);

  for (std::size_t axis = 0; axis < componentWorldEnds.size(); ++axis) {
    const glm::vec2 componentEnd = projectWorldPoint(windowViewport, view, componentWorldEnds[axis]);
    drawDashedLine(nvg, start, componentEnd, nvgColor(shadowColor, shadowOpacity), 3.5f * scale, scale);
    drawDashedLine(nvg, start, componentEnd, nvgColor(axisColors[axis], shadowOpacity), 1.5f * scale, scale);
  }

  if (glm::length(end - start) >= kMinimumArrowLength * scale) {
    drawArrow(nvg, start, end, nvgColor(shadowColor, shadowOpacity), 5.0f * scale, scale);
    drawArrow(nvg, start, end, nvgColor(color, opacity), 2.5f * scale, scale);
  }
  else {
    const float radius = 5.0f * scale;
    nvgBeginPath(nvg);
    nvgCircle(nvg, start.x, start.y, radius + 1.5f * scale);
    nvgStrokeWidth(nvg, 3.0f * scale);
    nvgStrokeColor(nvg, nvgColor(shadowColor, shadowOpacity));
    nvgStroke(nvg);
    nvgBeginPath(nvg);
    nvgCircle(nvg, start.x, start.y, radius);
    nvgStrokeWidth(nvg, 1.75f * scale);
    nvgStrokeColor(nvg, nvgColor(color, opacity));
    nvgStroke(nvg);
    nvgBeginPath(nvg);
    nvgCircle(nvg, start.x, start.y, 1.5f * scale);
    nvgFillColor(nvg, nvgColor(color, opacity));
    nvgFill(nvg);
  }

  nvgBeginPath(nvg);
  nvgCircle(nvg, start.x, start.y, 2.5f * scale);
  nvgFillColor(nvg, nvgColor(color, opacity));
  nvgFill(nvg);

  if (showParameters) {
    const float magnitude = glm::length(guide.displacementWorld);
    const std::array<std::string, 2> labels{
      "(" + fixed(guide.displacementWorld.x, precision) + ", " + fixed(guide.displacementWorld.y, precision) + ", " +
        fixed(guide.displacementWorld.z, precision) + ")",
      "|t| = " + fixed(magnitude, precision)};

    drawLabels(
      nvg,
      frameBounds,
      end + glm::vec2{10.0f, 8.0f} * scale,
      labels,
      nvgColor(color, opacity),
      shadowOpacity,
      scale);
  }

  nvgRestore(nvg);
}

void drawRotationGuide(
  NVGcontext* nvg,
  const FrameBounds& frameBounds,
  const Viewport& windowViewport,
  const View& view,
  const interaction::RotationGuide& guide,
  const glm::vec4& color,
  const float uiScale,
  const int precision,
  const bool showParameters)
{
  const float opacity = guide.presentation.opacity;
  if (!nvg || opacity <= 0.0f || frameBounds.bounds.width <= 0.0f || frameBounds.bounds.height <= 0.0f) {
    return;
  }

  const float scale = std::clamp(uiScale, 0.5f, 4.0f);
  const float shadowOpacity = opacity * std::clamp(color.a, 0.0f, 1.0f);
  const auto worldArc = interaction::rotationArcWorldPoints(guide, 48);
  std::vector<glm::vec2> arc;
  arc.reserve(worldArc.size());
  std::ranges::transform(worldArc, std::back_inserter(arc), [&](const glm::vec3& point) {
    return projectWorldPoint(windowViewport, view, point);
  });
  if (arc.empty()) {
    return;
  }

  const glm::vec2 center = projectWorldPoint(windowViewport, view, guide.centerWorld);
  const glm::vec4 shadowColor{0.0f, 0.0f, 0.0f, 0.84f};
  const NVGcolor shadow = nvgColor(shadowColor, shadowOpacity);
  const NVGcolor guideColor = nvgColor(color, opacity);
  beginGuideDrawing(nvg, frameBounds);

  strokeLine(nvg, center, arc.front(), shadow, 4.5f * scale);
  strokeLine(nvg, center, arc.back(), shadow, 4.5f * scale);
  strokePolyline(nvg, arc, shadow, 5.0f * scale);
  strokeLine(nvg, center, arc.front(), guideColor, 2.0f * scale);
  strokeLine(nvg, center, arc.back(), guideColor, 2.0f * scale);
  strokePolyline(nvg, arc, guideColor, 2.25f * scale);
  if (arc.size() >= 2) {
    const std::size_t arrowStart = arc.size() > 7 ? arc.size() - 7 : 0;
    drawArrow(nvg, arc[arrowStart], arc.back(), shadow, 4.5f * scale, scale);
    drawArrow(nvg, arc[arrowStart], arc.back(), guideColor, 2.0f * scale, scale);
  }
  drawCenterMarker(nvg, center, shadow, guideColor, scale);

  if (showParameters) {
    const interaction::RotationAxisAngle axisAngle = interaction::rotationAxisAngle(guide);
    const std::array<std::string, 2> labels{
      fixed(glm::degrees(axisAngle.angleRadians), precision) + "°",
      "axis = (" + fixed(axisAngle.axisWorld.x, precision) + ", " + fixed(axisAngle.axisWorld.y, precision) + ", " +
        fixed(axisAngle.axisWorld.z, precision) + ")"};
    drawLabels(nvg, frameBounds, arc.back() + glm::vec2{10.0f, 8.0f} * scale, labels, guideColor, shadowOpacity, scale);
  }

  nvgRestore(nvg);
}

void drawScaleGuide(
  NVGcontext* nvg,
  const FrameBounds& frameBounds,
  const Viewport& windowViewport,
  const View& view,
  const glm::vec3& slicePlaneOriginWorld,
  const interaction::ScaleGuide& guide,
  const glm::vec4& color,
  const float uiScale,
  const int precision,
  const bool showParameters)
{
  const float opacity = guide.presentation.opacity;
  if (!nvg || opacity <= 0.0f || frameBounds.bounds.width <= 0.0f || frameBounds.bounds.height <= 0.0f) {
    return;
  }

  const float scale = std::clamp(uiScale, 0.5f, 4.0f);
  const float shadowOpacity = opacity * std::clamp(color.a, 0.0f, 1.0f);
  const glm::vec3 slicePlaneNormalWorld = helper::worldDirection(view.camera(), Directions::View::Front);
  const auto initialOutline = projectWorldPoints(
    windowViewport,
    view,
    interaction::boxPlaneIntersectionOutline(guide.initialWorldCorners, slicePlaneOriginWorld, slicePlaneNormalWorld));
  const auto currentOutline = projectWorldPoints(
    windowViewport,
    view,
    interaction::boxPlaneIntersectionOutline(guide.currentWorldCorners, slicePlaneOriginWorld, slicePlaneNormalWorld));
  const glm::vec2 center = projectWorldPoint(windowViewport, view, guide.centerWorld);
  const glm::vec2 pointerStart = projectWorldPoint(windowViewport, view, guide.pointerStartWorld);
  const glm::vec2 pointerCurrent = projectWorldPoint(windowViewport, view, guide.pointerCurrentWorld);
  const glm::vec4 shadowColor{0.0f, 0.0f, 0.0f, 0.84f};
  const NVGcolor shadow = nvgColor(shadowColor, shadowOpacity);
  const NVGcolor guideColor = nvgColor(color, opacity);
  const NVGcolor initialShadow = nvgColor(glm::vec4{0.0f, 0.0f, 0.0f, 0.42f}, shadowOpacity);
  const NVGcolor initialColor = nvgColor(glm::vec4{color.r, color.g, color.b, color.a * 0.42f}, opacity);
  beginGuideDrawing(nvg, frameBounds);

  for (std::size_t i = 0; i < initialOutline.size(); ++i) {
    const std::size_t next = (i + 1) % initialOutline.size();
    drawDashedLine(nvg, initialOutline[i], initialOutline[next], initialShadow, 2.5f * scale, scale);
    drawDashedLine(nvg, initialOutline[i], initialOutline[next], initialColor, 1.0f * scale, scale);
  }
  if (currentOutline.size() >= 3) {
    strokePolyline(nvg, currentOutline, shadow, 4.0f * scale, true);
    strokePolyline(nvg, currentOutline, guideColor, 1.75f * scale, true);
  }

  if (glm::length(pointerCurrent - pointerStart) >= kMinimumArrowLength * scale) {
    drawArrow(nvg, pointerStart, pointerCurrent, shadow, 5.0f * scale, scale);
    drawArrow(nvg, pointerStart, pointerCurrent, guideColor, 2.5f * scale, scale);
  }
  drawCenterMarker(nvg, center, shadow, guideColor, scale);

  if (showParameters) {
    const glm::vec3 factors = interaction::relativeScaleFactors(guide);
    const std::array<std::string, 2> labels{
      "scale = (" + fixed(factors.x, precision) + ", " + fixed(factors.y, precision) + ", " +
        fixed(factors.z, precision) + ")",
      ""};
    drawLabels(
      nvg,
      frameBounds,
      pointerCurrent + glm::vec2{10.0f, 8.0f} * scale,
      labels,
      guideColor,
      shadowOpacity,
      scale);
  }

  nvgRestore(nvg);
}

void drawTransformationGuide(
  NVGcontext* nvg,
  const FrameBounds& frameBounds,
  const Viewport& windowViewport,
  const View& view,
  const glm::vec3& slicePlaneOriginWorld,
  const interaction::TransformationGuide& guide,
  const glm::vec4& color,
  const float uiScale,
  const int precision,
  const bool showParameters)
{
  std::visit(
    [&](const auto& typedGuide) {
      using Guide = std::decay_t<decltype(typedGuide)>;
      if constexpr (std::is_same_v<Guide, interaction::TranslationGuide>) {
        drawTranslationGuide(
          nvg,
          frameBounds,
          windowViewport,
          view,
          typedGuide,
          color,
          uiScale,
          precision,
          showParameters);
      }
      else if constexpr (std::is_same_v<Guide, interaction::RotationGuide>) {
        drawRotationGuide(
          nvg,
          frameBounds,
          windowViewport,
          view,
          typedGuide,
          color,
          uiScale,
          precision,
          showParameters);
      }
      else {
        drawScaleGuide(
          nvg,
          frameBounds,
          windowViewport,
          view,
          slicePlaneOriginWorld,
          typedGuide,
          color,
          uiScale,
          precision,
          showParameters);
      }
    },
    guide);
}

} // namespace rendering::vector_overlay
