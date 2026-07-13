#include "rendering/vector/ViewOverlayDrawing.h"

#include <nanovg.h>

#include <algorithm>

void drawEmptyThreeDViewHint(NVGcontext* nvg, const FrameBounds& miewportViewBounds)
{
  static const NVGcolor s_textColor(nvgRGBA(185, 185, 185, 215));
  static const NVGcolor s_shadowColor(nvgRGBA(20, 20, 20, 180));
  static const char* sk_hint = "Create an isosurface for the image to be rendered in this view";

  const float horizontalMargin = std::min(28.0f, std::max(8.0f, 0.08f * miewportViewBounds.bounds.width));
  const float textBoxWidth = std::max(40.0f, miewportViewBounds.bounds.width - 2.0f * horizontalMargin);
  const float x = miewportViewBounds.bounds.xoffset + horizontalMargin;
  const float centerY = miewportViewBounds.bounds.yoffset + 0.5f * miewportViewBounds.bounds.height;

  nvgFontSize(nvg, 16.0f);
  nvgFontFace(nvg, "robotoLight");
  nvgTextAlign(nvg, NVG_ALIGN_CENTER | NVG_ALIGN_TOP);
  nvgTextLineHeight(nvg, 1.25f);

  float bounds[4]{};
  nvgTextBoxBounds(nvg, x, centerY, textBoxWidth, sk_hint, nullptr, bounds);
  const float textHeight = bounds[3] - bounds[1];
  const float y = centerY - 0.5f * textHeight;

  nvgFontBlur(nvg, 2.0f);
  nvgFillColor(nvg, s_shadowColor);
  nvgTextBox(nvg, x, y, textBoxWidth, sk_hint, nullptr);

  nvgFontBlur(nvg, 0.0f);
  nvgFillColor(nvg, s_textColor);
  nvgTextBox(nvg, x, y, textBoxWidth, sk_hint, nullptr);
}
