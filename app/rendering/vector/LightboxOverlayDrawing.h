#pragma once

#include "common/Types.h"

#include <glm/fwd.hpp>

class AppData;
class View;

struct NVGcontext;

/**
 * @brief Draw the slice offset label for one lightbox tile.
 * @param nvg NanoVG context.
 * @param miewportViewBounds View bounds in miewport coordinates, meaning mouse-oriented/flipped viewport coordinates.
 * @param appData Application data containing render settings.
 * @param view Lightbox view being rendered.
 * @param unitReferenceLengthMm Reference length whose magnitude chooses the displayed unit.
 * @param color Label color as non-premultiplied RGBA.
 */
void drawLightboxOffsetLabel(
  NVGcontext* nvg,
  const FrameBounds& miewportViewBounds,
  AppData& appData,
  const View& view,
  double unitReferenceLengthMm,
  const glm::vec4& color);
