#pragma once

#include <imgui/imgui.h>

#include <algorithm>

namespace ui
{

/**
 * @brief Scale a reference logical pixel value by the current ImGui font scale.
 * @param value Value measured against Entropy's 16 px reference font size.
 * @return The value scaled for the current ImGui font size.
 */
inline float scaledPixel(float value)
{
  return value * (ImGui::GetFontSize() / 16.0f);
}

/**
 * @brief Scale a reference logical size by the current ImGui font scale.
 * @param x Width measured against Entropy's 16 px reference font size.
 * @param y Height measured against Entropy's 16 px reference font size.
 * @return The size scaled for the current ImGui font size.
 */
inline ImVec2 scaledSize(float x, float y)
{
  return ImVec2{scaledPixel(x), scaledPixel(y)};
}

/**
 * @brief Scale a reference logical size by the current ImGui font scale.
 * @param size Size measured against Entropy's 16 px reference font size.
 * @return The size scaled for the current ImGui font size.
 */
inline ImVec2 scaledSize(const ImVec2& size)
{
  return scaledSize(size.x, size.y);
}

/**
 * @brief Scale a default window size and clamp it to the main viewport work area.
 * @param width Reference width measured against Entropy's 16 px reference font size.
 * @param height Reference height measured against Entropy's 16 px reference font size.
 * @param maxViewportFraction Largest fraction of the viewport work area that the returned size may occupy.
 * @return Scaled size clamped to the current viewport work area when a viewport is available.
 */
inline ImVec2 viewportClampedScaledSize(float width, float height, float maxViewportFraction = 0.9f)
{
  ImVec2 size = scaledSize(width, height);
  if (const ImGuiViewport* viewport = ImGui::GetMainViewport()) {
    const float fraction = std::clamp(maxViewportFraction, 0.1f, 1.0f);
    size.x = std::min(size.x, viewport->WorkSize.x * fraction);
    size.y = std::min(size.y, viewport->WorkSize.y * fraction);
  }
  return size;
}

} // namespace ui
