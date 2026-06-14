#include "ui/Style.h"

#include <catch2/catch_test_macros.hpp>
#include <imgui/imgui.h>

#include <array>

namespace
{
bool sameColor(const ImVec4& a, const ImVec4& b)
{
  return a.x == b.x && a.y == b.y && a.z == b.z && a.w == b.w;
}
} // namespace

TEST_CASE("UI color presets initialize every ImGui color slot", "[ui][style]")
{
  static constexpr std::array<UiColorPreset, 9> sk_presets{
    UiColorPreset::EntropyDark,
    UiColorPreset::ImGuiDark,
    UiColorPreset::ImGuiClassic,
    UiColorPreset::ImGuiLight,
    UiColorPreset::SlateBlue,
    UiColorPreset::Graphite,
    UiColorPreset::DeepTeal,
    UiColorPreset::Midnight,
    UiColorPreset::SoftLight};

  constexpr ImVec4 sentinel(0.123f, 0.234f, 0.345f, 0.456f);

  for (const UiColorPreset preset : sk_presets) {
    ImGuiStyle style;
    for (ImVec4& color : style.Colors) {
      color = sentinel;
    }

    applyUiStylePreset(preset, &style);

    CAPTURE(uiColorPresetName(preset));
    for (int colorIndex = 0; colorIndex < ImGuiCol_COUNT; ++colorIndex) {
      INFO("Uninitialized ImGui color slot: " << ImGui::GetStyleColorName(colorIndex));
      CHECK_FALSE(sameColor(style.Colors[colorIndex], sentinel));
    }
  }
}

TEST_CASE("UI density presets update spacing and rounding", "[ui][style]")
{
  ImGuiStyle compact;
  ImGuiStyle standard;
  ImGuiStyle comfortable;

  applyUiDensityPreset(UiDensityPreset::Compact, &compact);
  applyUiDensityPreset(UiDensityPreset::Default, &standard);
  applyUiDensityPreset(UiDensityPreset::Comfortable, &comfortable);

  CHECK(compact.FramePadding.x < standard.FramePadding.x);
  CHECK(standard.FramePadding.x < comfortable.FramePadding.x);
  CHECK(compact.ItemSpacing.y < standard.ItemSpacing.y);
  CHECK(standard.ItemSpacing.y < comfortable.ItemSpacing.y);
  CHECK(compact.WindowRounding < standard.WindowRounding);
  CHECK(standard.WindowRounding < comfortable.WindowRounding);
  CHECK(compact.ScrollbarSize < standard.ScrollbarSize);
  CHECK(standard.ScrollbarSize < comfortable.ScrollbarSize);
}

TEST_CASE("Entropy Dark uses active header color for selected headers", "[ui][style]")
{
  ImGuiStyle style;
  applyUiStylePreset(UiColorPreset::EntropyDark, &style);

  CHECK(sameColor(style.Colors[ImGuiCol_Header], style.Colors[ImGuiCol_HeaderActive]));
  CHECK(sameColor(style.Colors[ImGuiCol_HeaderHovered], style.Colors[ImGuiCol_HeaderActive]));
}

TEST_CASE("UI window background opacity updates only window alpha", "[ui][style]")
{
  ImGuiStyle style;
  const ImVec4 originalWindowBg = style.Colors[ImGuiCol_WindowBg];
  const ImVec4 originalPopupBg = style.Colors[ImGuiCol_PopupBg];

  applyUiWindowBgOpacity(0.42f, &style);

  CHECK(style.Colors[ImGuiCol_WindowBg].x == originalWindowBg.x);
  CHECK(style.Colors[ImGuiCol_WindowBg].y == originalWindowBg.y);
  CHECK(style.Colors[ImGuiCol_WindowBg].z == originalWindowBg.z);
  CHECK(style.Colors[ImGuiCol_WindowBg].w == 0.42f);
  CHECK(sameColor(style.Colors[ImGuiCol_PopupBg], originalPopupBg));

  applyUiWindowBgOpacity(0.0f, &style);
  CHECK(style.Colors[ImGuiCol_WindowBg].w == 0.2f);

  applyUiWindowBgOpacity(2.0f, &style);
  CHECK(style.Colors[ImGuiCol_WindowBg].w == 1.0f);
}
