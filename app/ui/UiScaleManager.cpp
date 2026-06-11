#include "ui/UiScaleManager.h"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <cmath>
#include <utility>

void UiScaleManager::captureBaseStyle(const ImGuiStyle& style)
{
  m_baseStyle = style;
}

void UiScaleManager::setFontReloadCallback(FontReloadCallback callback)
{
  m_fontReloadCallback = std::move(callback);
}

void UiScaleManager::setUserScaleOverride(std::optional<float> scale)
{
  const float previousEffectiveScale = effectiveScale();

  if (scale) {
    scale = sanitizeScale(*scale);
  }

  if (m_userScaleOverride == scale) {
    return;
  }

  m_userScaleOverride = scale;
  applyEffectiveScale(previousEffectiveScale);
}

float UiScaleManager::contentScale() const
{
  return m_contentScale;
}

float UiScaleManager::effectiveScale() const
{
  return m_userScaleOverride.value_or(platformUiScale(m_contentScale));
}

std::optional<float> UiScaleManager::userScaleOverride() const
{
  return m_userScaleOverride;
}

bool UiScaleManager::applyContentScale(float scale)
{
  scale = sanitizeScale(scale);

  const float previousEffectiveScale = effectiveScale();
  if (m_contentScale == scale) {
    return false;
  }

  m_contentScale = scale;
  return applyEffectiveScale(previousEffectiveScale);
}

bool UiScaleManager::applyEffectiveScale(float previousScale)
{
  const float newEffectiveScale = effectiveScale();

  if (m_hasAppliedScale && std::abs(previousScale - newEffectiveScale) < 0.001f) {
    return false;
  }

  spdlog::info("Applying ImGui UI scale {}", newEffectiveScale);
  m_hasAppliedScale = true;

  ImGuiStyle& style = ImGui::GetStyle();
  style = m_baseStyle;
  style.ScaleAllSizes(newEffectiveScale);
  style.FontScaleDpi = newEffectiveScale;

  rebuildFonts(newEffectiveScale);
  return true;
}

void UiScaleManager::rebuildFonts(float scale)
{
  ImGuiIO& io = ImGui::GetIO();
  io.Fonts->Clear();

  if (m_fontReloadCallback) {
    m_fontReloadCallback(scale);
  }

  io.Fonts->Build();
}

void UiScaleManager::rebuildFontsForCurrentScale()
{
  rebuildFonts(effectiveScale());
}

float UiScaleManager::sanitizeScale(float scale)
{
  if (!std::isfinite(scale) || scale <= 0.0f) {
    return 1.0f;
  }

  return std::clamp(scale, 0.5f, 4.0f);
}

float UiScaleManager::platformUiScale(float contentScale)
{
#if defined(__APPLE__)
  (void)contentScale;
  return 1.0f;
#else
  return contentScale;
#endif
}
