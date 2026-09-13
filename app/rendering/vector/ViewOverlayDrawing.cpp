#include "rendering/vector/ViewOverlayDrawing.h"

#include <nanovg.h>

#include <algorithm>

void drawEmptyThreeDViewHint(NVGcontext* nvg, const FrameBounds& miewportViewBounds)
{
  // The 3D view can now show image planes and mesh content before an isosurface exists. Keep this function as the
  // future hook for a different empty-view hint, but do not draw the old isosurface-specific message.
  (void)nvg;
  (void)miewportViewBounds;
}
