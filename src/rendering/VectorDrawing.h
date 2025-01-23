#ifndef VECTOR_DRAWING_H
#define VECTOR_DRAWING_H

#include "common/Types.h"

#include <glm/fwd.hpp>
#include <glm/vec2.hpp>

#include <uuid.h>

#include <array>
#include <optional>
#include <utility>
#include <vector>

class AppData;
class View;
class Viewport;

struct NVGcontext;

using ImageSegPairs
  = std::vector<std::pair<std::optional<uuids::uuid>, std::optional<uuids::uuid> > >;

void startNvgFrame(NVGcontext* nvg, const Viewport& windowVP);

void endNvgFrame(NVGcontext* nvg);

void drawLoadingOverlay(NVGcontext* nvg, const Viewport& windowVP);

void drawWindowOutline(NVGcontext* nvg, const Viewport& windowVP);

enum class ViewOutlineMode
{
  Hovered,
  Selected,
  None
};

void drawViewOutline(
  NVGcontext* nvg, const FrameBounds& miewportViewBounds, const ViewOutlineMode& outlineMode
);

void drawImageViewIntersections(
  NVGcontext* nvg,
  const FrameBounds& miewportViewBounds,
  const glm::vec3& worldCrosshairs,
  AppData& appData,
  const View& view,
  const ImageSegPairs& I,
  bool renderInactiveImageIntersections
);

void drawAnatomicalLabels(
  NVGcontext* nvg,
  const FrameBounds& miewportViewBounds,
  bool isObliqueView,
  const glm::vec4& color,
  const AnatomicalLabelType& anatLabelType,
  const std::array<AnatomicalLabelPosInfo, 2>& labelPosInfo
);

void drawCircle(
  NVGcontext* nvg,
  const glm::vec2& miewportPos,
  float radius,
  const glm::vec4& fillColor,
  const glm::vec4& strokeColor,
  float strokeWidth
);

void drawText(
  NVGcontext* nvg,
  const glm::vec2& miewportPos,
  const std::string& centeredString,
  const std::string& offsetString,
  const glm::vec4& textColor,
  float offset,
  float fontSizePixels
);

void drawLandmarks(
  NVGcontext* nvg,
  const FrameBounds& miewportViewBounds,
  const glm::vec3& worldCrosshairs,
  AppData& appData,
  const View& view,
  const ImageSegPairs& I
);

void drawAnnotations(
  NVGcontext* nvg,
  const FrameBounds& miewportViewBounds,
  const glm::vec3& worldCrosshairs,
  AppData& appData,
  const View& view,
  const ImageSegPairs& I
);

void drawCrosshairs(
  NVGcontext* nvg,
  const FrameBounds& miewportViewBounds,
  const View& view,
  const glm::vec4& color,
  const std::array<AnatomicalLabelPosInfo, 2>& labelPosInfo
);

#endif // VECTOR_DRAWING_H
