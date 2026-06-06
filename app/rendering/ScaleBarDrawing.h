#pragma once

#include "common/Types.h"

#include <glm/fwd.hpp>

class View;
class Viewport;

struct NVGcontext;

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
  float marginPx);
