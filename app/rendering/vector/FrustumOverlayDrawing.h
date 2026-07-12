#pragma once

#include "common/Types.h"

#include <glm/fwd.hpp>

class View;
class Viewport;
namespace camera3d
{
struct FrustumSliceOverlay;
}

struct NVGcontext;

/**
 * @brief Draw the last-interacted 3D camera frustum overlay into a 2D view.
 * @param nvg NanoVG context.
 * @param miewportViewBounds View bounds in miewport coordinates, meaning mouse-oriented/flipped viewport coordinates.
 * @param windowViewport Full application window viewport.
 * @param view 2D view receiving the overlay.
 * @param overlay Precomputed frustum/view-plane intersection geometry.
 * @param color Overlay color as non-premultiplied RGBA.
 */
void drawThreeDCameraFrustumOverlay(
  NVGcontext* nvg,
  const FrameBounds& miewportViewBounds,
  const Viewport& windowViewport,
  const View& view,
  const camera3d::FrustumSliceOverlay& overlay,
  const glm::vec4& color);
