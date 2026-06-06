#include "ImGuiPaletteButton.h"

#include <imgui/imgui_internal.h>

#include <glm/glm.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/color_space.hpp>

#include <algorithm>
#include <cmath>

namespace ImGui
{

bool paletteButton(
  const char* label,
  const std::vector<glm::vec4>& colors,
  bool inverted,
  bool quantize,
  int quantizationLevels,
  const glm::vec3& hsvModFactors,
  const ImVec2& size)
{
  ImGuiWindow* window = GetCurrentWindow();

  if (!window) {
    return false;
  }
  if (window->SkipItems) {
    return false;
  }
  if (!GImGui) {
    return false;
  }

  const ImGuiID id = window->GetID(label);

  const float lineH = (window->DC.CurrLineSize.y <= 0)
                        ? ((window->DC.PrevLineSize.y <= 0) ? size.y : window->DC.PrevLineSize.y)
                        : (window->DC.CurrLineSize.y);

  const ImRect bb(
    ImVec2(window->DC.CursorPos.x, window->DC.CursorPos.y),
    ImVec2(window->DC.CursorPos.x + size.x, window->DC.CursorPos.y + lineH));

  ItemSize(bb);

  if (!ItemAdd(bb, id)) return false;

  const float borderY = (lineH < size.y) ? 0.0f : 0.5f * (lineH - size.y);

  const ImVec2 posMin = ImVec2(bb.Min.x, bb.Min.y + borderY);
  const ImVec2 posMax = ImVec2(bb.Max.x, bb.Max.y - borderY);
  const int width = static_cast<int>(posMax.x - posMin.x);

  ImDrawList* drawList = ImGui::GetWindowDrawList();

  auto renderFilledRects = [&](const float alpha) {
    const float step = static_cast<float>(width) / static_cast<float>(colors.size());

    for (std::size_t i = 0; i < colors.size(); ++i) {
      const float minX = posMin.x + static_cast<float>(i) * step;

      std::size_t index = i;

      if (quantize) {
        const float normIndex = static_cast<float>(i) / static_cast<float>(colors.size() - 1);

        index = static_cast<std::size_t>(
          std::min(std::max(std::floor(quantizationLevels * normIndex) / (quantizationLevels - 1), 0.0f), 1.0f) *
          (colors.size() - 1));
      }

      if (inverted) {
        index = colors.size() - 1 - index;
      }

      glm::vec3 colorHsv = glm::hsvColor(glm::vec3{colors[index]});

      colorHsv[0] += 360.0f * hsvModFactors[0];
      colorHsv[0] = std::fmod(colorHsv[0], 360.0f);

      colorHsv[1] *= hsvModFactors[1];
      colorHsv[2] *= hsvModFactors[2];

      const glm::vec3 colorRgb = glm::rgbColor(colorHsv);

      drawList->AddRectFilled(
        ImVec2(minX, posMin.y),
        ImVec2(minX + step, posMax.y),
        IM_COL32(255.0f * colorRgb.r, 255.0f * colorRgb.g, 255.0f * colorRgb.b, 255.0f * alpha * colors[index].a));
    }
  };

  renderFilledRects(ImGui::GetStyle().Alpha);

  bool hovered, held;
  const bool pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held, 0);

  const ImU32 col = GetColorU32(
    (held && hovered) ? ImGuiCol_ButtonActive
    : hovered         ? ImGuiCol_ButtonHovered
                      : ImGuiCol_Button);
  drawList->AddRect(posMin, posMax, col, 0.0f, 0, 0.5f);

  return pressed;
}

} // namespace ImGui
