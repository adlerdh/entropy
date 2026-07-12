#pragma once

#include "common/Types.h"

#include <glm/fwd.hpp>

class View;
class Viewport;

struct NVGcontext;

/**
 * @brief Draw a scale bar overlay for the given view using NanoVG.
 *
 * The scale bar length is computed from the view camera, the view-to-window transform, and the requested target size.
 * Nothing is drawn when the view cannot contain a readable scale bar with the configured target and margin.
 *
 * @param nvg NanoVG context used for vector overlay drawing.
 * @param miewportViewBounds View bounds in miewport coordinates, meaning mouse-oriented/flipped viewport coordinates.
 * @param windowViewport Window viewport used to convert view pixels into world spacing.
 * @param view View whose camera defines the world-to-screen scale.
 * @param color Scale bar and label color as non-premultiplied RGBA.
 * @param position Scale bar placement inside the view.
 * @param orientation Scale bar orientation.
 * @param ticks Tick mark style.
 * @param targetFraction Target fraction of the view width or height before nice-length rounding.
 * @param marginPx Margin from the selected view edge in screen pixels.
 * @param lengthPrecision Number of significant digits used when formatting the physical length label.
 */
void drawScaleBar(
  NVGcontext* nvg,
  const FrameBounds& miewportViewBounds,
  const Viewport& windowViewport,
  const View& view,
  const glm::vec4& color,
  ScaleBarPosition position,
  ScaleBarOrientation orientation,
  ScaleBarTicks ticks,
  float targetFraction,
  float marginPx,
  int lengthPrecision);
