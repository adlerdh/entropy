#pragma once

#include "common/Types.h"

struct NVGcontext;

/**
 * @brief Draw the centered hint shown in an empty 3D view when no isosurface can be rendered.
 *
 * @param nvg NanoVG context.
 * @param miewportViewBounds View bounds in miewport coordinates, meaning mouse-oriented/flipped viewport coordinates.
 */
void drawEmptyThreeDViewHint(NVGcontext* nvg, const FrameBounds& miewportViewBounds);
