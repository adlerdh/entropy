#pragma once

#include <glm/vec2.hpp>
#include <glm/vec4.hpp>
#include <imgui/imgui.h>

namespace ui::toolbars
{

inline const ImVec4 k_darkTextColor(0.0f, 0.0f, 0.0f, 1.0f);
inline const ImVec4 k_lightTextColor(1.0f, 1.0f, 1.0f, 1.0f);

inline const ImGuiWindowFlags k_toolbarWindowFlags =
  0 | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoResize |
  ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoDocking;

/**
 * @brief Render the common toolbar placement context menu.
 */
inline void renderPlacementContextMenu(int& corner, bool& /*isHoriz*/)
{
  if (ImGui::BeginMenu("Position")) {
    if (ImGui::MenuItem("Custom", nullptr, corner == -1)) corner = -1;
    if (ImGui::MenuItem("Top-left", nullptr, corner == 0)) corner = 0;
    if (ImGui::MenuItem("Top-right", nullptr, corner == 1)) corner = 1;
    if (ImGui::MenuItem("Bottom-left", nullptr, corner == 2)) corner = 2;
    if (ImGui::MenuItem("Bottom-right", nullptr, corner == 3)) corner = 3;
    ImGui::EndMenu();
  }
}

/**
 * @brief Return the scaled toolbar button size for the current ImGui font scale.
 */
inline ImVec2 scaledToolbarButtonSize(const glm::vec2& contentScale)
{
  static const ImVec2 k_toolbarButtonSize(32, 32);
  (void)contentScale;
  const float scale = ImGui::GetFontSize() / 16.0f;
  return ImVec2{scale * k_toolbarButtonSize.x, scale * k_toolbarButtonSize.y};
}

/**
 * @brief Return the scaled toolbar edge padding for the current ImGui font scale.
 */
inline ImVec2 scaledPad(const glm::vec2& contentScale)
{
  static constexpr float k_pad = 8.0f;
  (void)contentScale;
  const float scale = ImGui::GetFontSize() / 16.0f;
  return ImVec2{scale * k_pad, scale * k_pad};
}

/**
 * @brief Return a toolbar anchor position for a bottom-left-origin viewport.
 *
 * @param viewport Bounds in window coordinates: left, bottom, width, height.
 * @param windowHeight Height of the containing window in the same coordinate system.
 * @param corner 0 top-left, 1 top-right, 2 bottom-left, 3 bottom-right.
 * @param pad Padding from the selected viewport corner.
 * @return ImGui top-left-origin window position suitable for SetNextWindowPos().
 */
inline ImVec2
toolbarWindowPositionForViewport(const glm::vec4& viewport, float windowHeight, int corner, const ImVec2& pad)
{
  const float left = viewport.x;
  const float right = viewport.x + viewport.z;
  const float top = windowHeight - (viewport.y + viewport.w);
  const float bottom = windowHeight - viewport.y;

  return ImVec2{(corner & 1) ? right - pad.x : left + pad.x, (corner & 2) ? bottom - pad.y : top + pad.y};
}

} // namespace ui::toolbars
