#pragma once

#include <glm/vec2.hpp>
#include <imgui/imgui.h>

namespace entropy::ui::toolbars
{

inline const ImVec4 k_darkTextColor(0.0f, 0.0f, 0.0f, 1.0f);
inline const ImVec4 k_lightTextColor(1.0f, 1.0f, 1.0f, 1.0f);

inline const ImGuiWindowFlags k_toolbarWindowFlags =
  0 | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoResize |
  ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoNav;

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

} // namespace entropy::ui::toolbars
