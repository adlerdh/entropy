#include "ui/UiScaleManager.h"
#include "ui/LinuxUiScale.h"
#include "ui/windows/InspectionWindowSizing.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <imgui.h>

#include <cmath>
#include <initializer_list>
#include <optional>
#include <string>

namespace
{
struct Context
{
  Context()
  {
    ImGui::CreateContext();
    ImGui::GetIO().IniFilename = nullptr;
  }
  ~Context()
  {
    ImGui::DestroyContext();
  }
};
} // namespace

TEST_CASE("Monitor moves and scale polling preserve noncumulative inspector sizing", "[workflow][ui][platform][scale]")
{
  Context context;
  UiScaleManager scale;
  scale.captureBaseStyle(ImGui::GetStyle());
  const auto originalPadding = ImGui::GetStyle().WindowPadding;
  for (float live : {1.0f, 2.0f, 1.25f, 1.5f, 2.0f, 1.0f}) {
    const float resolved = ui::linux_ui_scale::liveOrFallbackScale(live, 2.0f);
    CHECK(resolved == live);
    scale.applyContentScale(resolved);
    const auto padding = ImGui::GetStyle().WindowPadding;
    for (int poll = 0; poll < 20; ++poll) {
      CHECK_FALSE(scale.applyContentScale(resolved));
      CHECK(ImGui::GetStyle().WindowPadding.x == padding.x);
      CHECK(ImGui::GetStyle().WindowPadding.y == padding.y);
    }
    for (float height : {120.0f, 240.0f, 720.0f, 1440.0f}) {
      const auto fitted =
        ui::fittedInspectionWindowHeight(10000, height, 120 * scale.effectiveScale(), 480 * scale.effectiveScale());
      CHECK(fitted >= 0);
      CHECK(fitted <= height * 0.45f);
    }
  }
  CHECK(ImGui::GetStyle().WindowPadding.x == originalPadding.x);
  CHECK(ImGui::GetStyle().WindowPadding.y == originalPadding.y);
}

TEST_CASE(
  "Small inspector layouts with long labels produce finite clipped draw geometry",
  "[workflow][ui][platform][layout]")
{
  const float userScale = GENERATE(1.0f, 1.25f, 1.5f, 2.0f);
  const float width = GENERATE(240.0f, 400.0f, 1024.0f);
  Context context;
  UiScaleManager scale;
  scale.captureBaseStyle(ImGui::GetStyle());
  scale.setUserScaleOverride(userScale);
  auto& io = ImGui::GetIO();
  io.DisplaySize = {width, 480};
  io.DeltaTime = 1.0f / 60.0f;
  unsigned char* pixels = nullptr;
  int atlasWidth = 0, atlasHeight = 0;
  io.Fonts->GetTexDataAsRGBA32(&pixels, &atlasWidth, &atlasHeight);
  const std::string longLabel = "Image properties / Eigenschaften / Propriétés / " + std::string(512, 'W');
  for (int frame = 0; frame < 3; ++frame) {
    ImGui::NewFrame();
    ImGui::SetNextWindowPos({0, 0});
    ImGui::SetNextWindowSize({width, 480});
    ImGui::Begin("Inspector###stable-inspector", nullptr, ImGuiWindowFlags_NoSavedSettings);
    ImGui::TextWrapped("%s", longLabel.c_str());
    ImGui::Button("Apply changes");
    ImGui::End();
    ImGui::Render();
    const auto* draw = ImGui::GetDrawData();
    REQUIRE(draw);
    CHECK(draw->Valid);
    for (const auto* list : draw->CmdLists) {
      for (const auto& vertex : list->VtxBuffer) {
        CHECK(std::isfinite(vertex.pos.x));
        CHECK(std::isfinite(vertex.pos.y));
      }
      for (const auto& command : list->CmdBuffer) {
        CHECK(command.ClipRect.x >= 0);
        CHECK(command.ClipRect.y >= 0);
        CHECK(command.ClipRect.z <= width);
        CHECK(command.ClipRect.w <= 480);
      }
    }
  }
}
