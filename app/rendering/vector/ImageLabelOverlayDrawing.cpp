#include "rendering/vector/ImageLabelOverlayDrawing.h"

#include <nanovg.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <string>
#include <string_view>

namespace rendering::vector_overlay
{
namespace
{
constexpr std::string_view k_fontFace = "robotoLight";
constexpr float k_baseFontSize = 14.0f;
constexpr float k_baseFramePadding = 4.0f;
constexpr float k_controlToLabelGap = 7.5f;
constexpr float k_gapScaleCorrection = 7.0f;
constexpr float k_baseSwatchSize = 8.0f;
constexpr float k_baseGap = 4.0f;
constexpr float k_baseEntryGap = 8.0f;
constexpr float k_baseBadgeFontSize = 9.0f;
constexpr float k_baseBadgePaddingX = 3.0f;
constexpr float k_baseBadgeHeight = 13.0f;

struct Metrics
{
  float scale;
  float fontSize;
  float lineHeight;
  float padding;
  float swatchSize;
  float gap;
  float entryGap;
  float badgeFontSize;
  float badgePaddingX;
  float badgeHeight;
  float shadowBlur;
  float shadowOffset;
};

Metrics metrics(float uiScale)
{
  const float scale = std::clamp(uiScale, 0.5f, 4.0f);
  const float fontSize = k_baseFontSize * scale;
  return Metrics{
    .scale = scale,
    .fontSize = fontSize,
    .lineHeight = std::ceil(fontSize * 1.35f),
    .padding = k_baseFramePadding * scale,
    .swatchSize = k_baseSwatchSize * scale,
    .gap = k_baseGap * scale,
    .entryGap = k_baseEntryGap * scale,
    .badgeFontSize = k_baseBadgeFontSize * scale,
    .badgePaddingX = k_baseBadgePaddingX * scale,
    .badgeHeight = k_baseBadgeHeight * scale,
    .shadowBlur = std::max(0.35f, 0.5f * scale),
    .shadowOffset = 0.7f * scale};
}

float textWidth(NVGcontext* nvg, std::string_view text)
{
  std::array<float, 4> bounds{};
  nvgTextBounds(nvg, 0.0f, 0.0f, text.data(), text.data() + text.size(), bounds.data());
  return bounds[2] - bounds[0];
}

float badgeWidth(NVGcontext* nvg, std::string_view text, const Metrics& m)
{
  nvgFontSize(nvg, m.badgeFontSize);
  return textWidth(nvg, text) + 2.0f * m.badgePaddingX;
}

float entryWidth(NVGcontext* nvg, const ImageLabelEntry& entry, const Metrics& m)
{
  nvgFontSize(nvg, m.fontSize);
  float width = m.swatchSize + m.gap + textWidth(nvg, entry.displayName);
  for (const std::string_view badge : imageRoleBadgeLabels(entry)) {
    if (!badge.empty()) {
      width += m.gap + badgeWidth(nvg, badge, m);
    }
  }
  return width;
}

void drawShadowedText(NVGcontext* nvg, float x, float y, std::string_view text, const Metrics& m)
{
  nvgFontSize(nvg, m.fontSize);
  nvgFontBlur(nvg, m.shadowBlur);
  nvgFillColor(nvg, nvgRGBA(0, 0, 0, 250));
  for (const std::array<float, 2>& offset : std::array{
         std::array{-m.shadowOffset, -m.shadowOffset},
         std::array{m.shadowOffset, -m.shadowOffset},
         std::array{-m.shadowOffset, m.shadowOffset},
         std::array{m.shadowOffset, m.shadowOffset}})
  {
    nvgText(nvg, x + offset[0], y + offset[1], text.data(), text.data() + text.size());
  }

  nvgFontBlur(nvg, 0.0f);
  nvgFillColor(nvg, nvgRGBA(220, 220, 220, 255));
  nvgText(nvg, x, y, text.data(), text.data() + text.size());
}

float drawBadge(NVGcontext* nvg, float x, float lineTop, std::string_view text, const Metrics& m)
{
  const float width = badgeWidth(nvg, text, m);
  const float y = lineTop + 0.5f * (m.lineHeight - m.badgeHeight);

  nvgBeginPath(nvg);
  nvgRoundedRect(nvg, x, y, width, m.badgeHeight, 2.5f * m.scale);
  nvgFillColor(nvg, nvgRGBA(25, 25, 25, 195));
  nvgFill(nvg);
  nvgStrokeWidth(nvg, std::max(1.0f, 0.75f * m.scale));
  nvgStrokeColor(nvg, nvgRGBA(220, 220, 220, 145));
  nvgStroke(nvg);

  nvgFontSize(nvg, m.badgeFontSize);
  nvgFontBlur(nvg, 0.0f);
  nvgTextAlign(nvg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
  nvgFillColor(nvg, nvgRGBA(230, 230, 230, 255));
  nvgText(nvg, x + 0.5f * width, y + 0.5f * m.badgeHeight, text.data(), text.data() + text.size());
  nvgTextAlign(nvg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
  return width;
}

void drawEntry(NVGcontext* nvg, float x, float lineTop, const ImageLabelEntry& entry, const Metrics& m)
{
  const float centerY = lineTop + 0.5f * m.lineHeight;
  const float swatchY = lineTop + 0.5f * (m.lineHeight - m.swatchSize);
  const ImageSwatchMode swatchMode = imageSwatchMode(entry);

  if (ImageSwatchMode::Opaque == swatchMode) {
    nvgBeginPath(nvg);
    nvgRoundedRect(nvg, x, swatchY, m.swatchSize, m.swatchSize, 1.5f * m.scale);
    nvgFillColor(
      nvg,
      nvgRGBAf(
        std::clamp(entry.identificationColor.r, 0.0f, 1.0f),
        std::clamp(entry.identificationColor.g, 0.0f, 1.0f),
        std::clamp(entry.identificationColor.b, 0.0f, 1.0f),
        1.0f));
    nvgFill(nvg);
  }
  else {
    const float half = 0.5f * m.swatchSize;
    const std::array<NVGcolor, 2> checkerColors{nvgRGBA(210, 210, 210, 255), nvgRGBA(115, 115, 115, 255)};
    for (int row = 0; row < 2; ++row) {
      for (int column = 0; column < 2; ++column) {
        nvgBeginPath(nvg);
        nvgRect(nvg, x + static_cast<float>(column) * half, swatchY + static_cast<float>(row) * half, half, half);
        nvgFillColor(nvg, checkerColors[static_cast<std::size_t>((row + column) % 2)]);
        nvgFill(nvg);
      }
    }

    if (ImageSwatchMode::Translucent == swatchMode) {
      nvgBeginPath(nvg);
      nvgRect(nvg, x, swatchY, m.swatchSize, m.swatchSize);
      nvgFillColor(
        nvg,
        nvgRGBAf(
          std::clamp(entry.identificationColor.r, 0.0f, 1.0f),
          std::clamp(entry.identificationColor.g, 0.0f, 1.0f),
          std::clamp(entry.identificationColor.b, 0.0f, 1.0f),
          std::clamp(entry.effectiveOpacity, 0.0f, 1.0f)));
      nvgFill(nvg);
    }
  }

  nvgBeginPath(nvg);
  nvgRoundedRect(nvg, x, swatchY, m.swatchSize, m.swatchSize, 1.5f * m.scale);
  nvgStrokeWidth(nvg, std::max(1.0f, 0.75f * m.scale));
  nvgStrokeColor(nvg, nvgRGBA(0, 0, 0, 225));
  nvgStroke(nvg);

  if (ImageSwatchMode::Hidden == swatchMode) {
    const float inset = std::max(1.0f, 1.25f * m.scale);
    nvgBeginPath(nvg);
    nvgMoveTo(nvg, x + inset, swatchY + inset);
    nvgLineTo(nvg, x + m.swatchSize - inset, swatchY + m.swatchSize - inset);
    nvgMoveTo(nvg, x + m.swatchSize - inset, swatchY + inset);
    nvgLineTo(nvg, x + inset, swatchY + m.swatchSize - inset);
    nvgStrokeWidth(nvg, std::max(1.0f, 1.4f * m.scale));
    nvgStrokeColor(
      nvg,
      nvgRGBAf(
        std::clamp(entry.identificationColor.r, 0.0f, 1.0f),
        std::clamp(entry.identificationColor.g, 0.0f, 1.0f),
        std::clamp(entry.identificationColor.b, 0.0f, 1.0f),
        1.0f));
    nvgStroke(nvg);
  }

  x += m.swatchSize + m.gap;
  drawShadowedText(nvg, x, centerY, entry.displayName, m);
  nvgFontSize(nvg, m.fontSize);
  x += textWidth(nvg, entry.displayName);

  for (const std::string_view badge : imageRoleBadgeLabels(entry)) {
    if (!badge.empty()) {
      x += m.gap;
      x += drawBadge(nvg, x, lineTop, badge, m);
    }
  }
}
} // namespace

void drawImageLabelOverlay(
  NVGcontext* nvg,
  const FrameBounds& frameBounds,
  std::span<const ImageLabelEntry> entries,
  float uiScale,
  float controlBottomOffset)
{
  if (!nvg || entries.empty() || frameBounds.bounds.width <= 0.0f || frameBounds.bounds.height <= 0.0f) {
    return;
  }

  const Metrics m = metrics(uiScale);
  const float left = frameBounds.bounds.xoffset + m.padding;
  const float right = frameBounds.bounds.xoffset + frameBounds.bounds.width - m.padding;
  float x = left;
  const float textCenteringInset = 0.5f * (m.lineHeight - m.fontSize);
  const float scaleCorrection = k_gapScaleCorrection * std::max(0.0f, m.scale - 1.0f);
  float y = frameBounds.bounds.yoffset + std::max(0.0f, controlBottomOffset) + k_controlToLabelGap -
            textCenteringInset - scaleCorrection;

  nvgSave(nvg);
  nvgScissor(
    nvg,
    frameBounds.bounds.xoffset,
    frameBounds.bounds.yoffset,
    frameBounds.bounds.width,
    frameBounds.bounds.height);
  nvgFontFace(nvg, k_fontFace.data());
  nvgTextAlign(nvg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);

  for (const ImageLabelEntry& entry : entries) {
    const float width = entryWidth(nvg, entry, m);
    if (x > left && x + width > right) {
      x = left;
      y += m.lineHeight;
    }
    if (y + m.lineHeight > frameBounds.bounds.yoffset + frameBounds.bounds.height) {
      break;
    }

    drawEntry(nvg, x, y, entry, m);
    x += width + m.entryGap;
  }

  nvgRestore(nvg);
}

} // namespace rendering::vector_overlay
