#include "ui/Style.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <imgui/imgui.h>

#include <array>

namespace
{
bool sameColor(const ImVec4& a, const ImVec4& b)
{
  return a.x == b.x && a.y == b.y && a.z == b.z && a.w == b.w;
}

struct ImGuiContextScope
{
  ImGuiContextScope()
  {
    ImGui::CreateContext();
  }

  ~ImGuiContextScope()
  {
    ImGui::DestroyContext();
  }
};
} // namespace

TEST_CASE("UI preset names cover all public presets", "[ui][style]")
{
  CHECK(uiColorPresetName(UiColorPreset::EntropyDark) == std::string("Entropy Dark"));
  CHECK(uiColorPresetName(UiColorPreset::ImGuiDark) == std::string("ImGui Dark"));
  CHECK(uiColorPresetName(UiColorPreset::ImGuiClassic) == std::string("ImGui Classic"));
  CHECK(uiColorPresetName(UiColorPreset::ImGuiLight) == std::string("ImGui Light"));
  CHECK(uiColorPresetName(UiColorPreset::SlateBlue) == std::string("Slate Blue"));
  CHECK(uiColorPresetName(UiColorPreset::Graphite) == std::string("Graphite"));
  CHECK(uiColorPresetName(UiColorPreset::DeepTeal) == std::string("Deep Teal"));
  CHECK(uiColorPresetName(UiColorPreset::Midnight) == std::string("Midnight"));
  CHECK(uiColorPresetName(UiColorPreset::SoftLight) == std::string("Soft Light"));

  CHECK(uiDensityPresetName(UiDensityPreset::Compact) == std::string("Compact"));
  CHECK(uiDensityPresetName(UiDensityPreset::Default) == std::string("Default"));
  CHECK(uiDensityPresetName(UiDensityPreset::Comfortable) == std::string("Comfortable"));
}

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

  CHECK(compact.WindowPadding.x == 6.0f);
  CHECK(standard.WindowPadding.x == 8.0f);
  CHECK(comfortable.WindowPadding.x == 12.0f);
}

TEST_CASE("Entropy Dark uses active header color for selected headers", "[ui][style]")
{
  ImGuiStyle style;
  applyUiStylePreset(UiColorPreset::EntropyDark, &style);

  CHECK(sameColor(style.Colors[ImGuiCol_Header], style.Colors[ImGuiCol_HeaderActive]));
  CHECK(sameColor(style.Colors[ImGuiCol_HeaderHovered], style.Colors[ImGuiCol_HeaderActive]));
}

TEST_CASE("UI color presets use active header color for active title bars", "[ui][style]")
{
  static constexpr std::array<UiColorPreset, 9> presets{
    UiColorPreset::EntropyDark,
    UiColorPreset::ImGuiDark,
    UiColorPreset::ImGuiClassic,
    UiColorPreset::ImGuiLight,
    UiColorPreset::SlateBlue,
    UiColorPreset::Graphite,
    UiColorPreset::DeepTeal,
    UiColorPreset::Midnight,
    UiColorPreset::SoftLight};

  for (const UiColorPreset preset : presets) {
    ImGuiStyle style;
    applyUiStylePreset(preset, &style);

    CAPTURE(uiColorPresetName(preset));
    CHECK(sameColor(style.Colors[ImGuiCol_TitleBgActive], style.Colors[ImGuiCol_HeaderActive]));
  }
}

TEST_CASE("UI color presets use active header color for selected tabs", "[ui][style]")
{
  static constexpr std::array<UiColorPreset, 9> presets{
    UiColorPreset::EntropyDark,
    UiColorPreset::ImGuiDark,
    UiColorPreset::ImGuiClassic,
    UiColorPreset::ImGuiLight,
    UiColorPreset::SlateBlue,
    UiColorPreset::Graphite,
    UiColorPreset::DeepTeal,
    UiColorPreset::Midnight,
    UiColorPreset::SoftLight};

  for (const UiColorPreset preset : presets) {
    ImGuiStyle style;
    applyUiStylePreset(preset, &style);

    CAPTURE(uiColorPresetName(preset));
    CHECK(sameColor(style.Colors[ImGuiCol_TabSelected], style.Colors[ImGuiCol_HeaderActive]));
    CHECK(sameColor(style.Colors[ImGuiCol_TabDimmedSelected], style.Colors[ImGuiCol_HeaderActive]));
  }
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

TEST_CASE("legacy custom styles can be applied through current ImGui style", "[ui][style]")
{
  ImGuiContextScope context;

  applyCustomLightStyle(false, 0.5f);
  CHECK(ImGui::GetStyle().Alpha == Catch::Approx(1.0f));
  CHECK(ImGui::GetStyle().FrameRounding == Catch::Approx(3.0f));
  CHECK(ImGui::GetStyle().Colors[ImGuiCol_WindowBg].w == Catch::Approx(0.47f));

  applyCustomLightStyle(true, 0.5f);
  CHECK(ImGui::GetStyle().Colors[ImGuiCol_WindowBg].w == Catch::Approx(0.47f));

  applyCustomDarkStyle2();
  CHECK(ImGui::GetStyle().Colors[ImGuiCol_WindowBg].x == Catch::Approx(0.180f));
  CHECK(ImGui::GetStyle().Colors[ImGuiCol_ScrollbarGrabActive].x == Catch::Approx(1.000f));
}

TEST_CASE("custom soft style applies spacing and light control colors", "[ui][style]")
{
  ImGuiStyle style;

  applyCustomSoftStyle(&style);

  CHECK(style.DisplaySafeAreaPadding.x == Catch::Approx(0.0f));
  CHECK(style.WindowPadding.x == Catch::Approx(4.0f));
  CHECK(style.FramePadding.x == Catch::Approx(8.0f));
  CHECK(style.ScrollbarSize == Catch::Approx(20.0f));
  CHECK(style.WindowRounding == Catch::Approx(0.0f));
  CHECK(style.Colors[ImGuiCol_WindowBg].x == Catch::Approx(0.95f));
  CHECK(style.Colors[ImGuiCol_ButtonActive].y == Catch::Approx(0.47f));
}

TEST_CASE("style helpers update the current ImGui style when no destination is supplied", "[ui][style]")
{
  ImGuiContextScope context;

  applyCustomDarkStyle();
  CHECK(ImGui::GetStyle().WindowRounding == Catch::Approx(5.0f));
  CHECK(ImGui::GetStyle().Colors[ImGuiCol_WindowBg].x == Catch::Approx(0.06f));

  applyUiDensityPreset(UiDensityPreset::Comfortable);
  CHECK(ImGui::GetStyle().FramePadding.x == Catch::Approx(10.0f));

  applyUiStylePreset(UiColorPreset::SoftLight);
  CHECK(ImGui::GetStyle().Colors[ImGuiCol_WindowBg].x == Catch::Approx(0.90f));

  applyUiWindowBgOpacity(0.37f);
  CHECK(ImGui::GetStyle().Colors[ImGuiCol_WindowBg].w == Catch::Approx(0.37f));
}
