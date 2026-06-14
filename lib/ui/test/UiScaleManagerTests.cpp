#include "ui/UiScaleManager.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <limits>

namespace
{
float expectedAutoScale(float contentScale)
{
#if defined(__APPLE__)
  // macOS Retina content scale controls framebuffer sharpness, not logical UI size.
  (void)contentScale;
  return 1.0f;
#else
  // Windows and Linux use OS content scale as the automatic logical UI scale.
  return contentScale;
#endif
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

TEST_CASE("UI scale manager applies auto content scale", "[ui][scale]")
{
  ImGuiContextScope context;
  UiScaleManager manager;
  int reloadCount = 0;

  manager.captureBaseStyle(ImGui::GetStyle());
  manager.setFontReloadCallback([&reloadCount](float) { ++reloadCount; });

  REQUIRE(manager.applyContentScale(2.0f));
  REQUIRE(manager.contentScale() == Catch::Approx(2.0f));
  REQUIRE(manager.effectiveScale() == Catch::Approx(expectedAutoScale(2.0f)));
  REQUIRE(reloadCount == 1);
}

TEST_CASE("UI scale manager ignores tiny content scale jitter", "[ui][scale]")
{
  ImGuiContextScope context;
  UiScaleManager manager;
  int reloadCount = 0;

  manager.captureBaseStyle(ImGui::GetStyle());
  manager.setFontReloadCallback([&reloadCount](float) { ++reloadCount; });

  REQUIRE(manager.applyContentScale(2.0f));
  REQUIRE_FALSE(manager.applyContentScale(2.0005f));
  REQUIRE(reloadCount == 1);
}

TEST_CASE("UI scale manager keeps manual override independent from content scale", "[ui][scale]")
{
  ImGuiContextScope context;
  UiScaleManager manager;
  int reloadCount = 0;

  manager.captureBaseStyle(ImGui::GetStyle());
  manager.setFontReloadCallback([&reloadCount](float) { ++reloadCount; });

  manager.setUserScaleOverride(1.5f);
  REQUIRE(manager.effectiveScale() == Catch::Approx(1.5f));
  REQUIRE(reloadCount == 1);

  REQUIRE_FALSE(manager.applyContentScale(2.0f));
  REQUIRE(manager.contentScale() == Catch::Approx(2.0f));
  REQUIRE(manager.effectiveScale() == Catch::Approx(1.5f));
  REQUIRE(reloadCount == 1);

  manager.setUserScaleOverride(std::nullopt);
  REQUIRE(manager.effectiveScale() == Catch::Approx(expectedAutoScale(2.0f)));
  REQUIRE(reloadCount == 2);
}

TEST_CASE("UI scale manager sanitizes invalid and out-of-range scales", "[ui][scale]")
{
  ImGuiContextScope context;
  UiScaleManager manager;
  float lastReloadScale = 0.0f;

  manager.captureBaseStyle(ImGui::GetStyle());
  manager.setFontReloadCallback([&lastReloadScale](float scale) { lastReloadScale = scale; });

  manager.setUserScaleOverride(-2.0f);
  CHECK(manager.userScaleOverride() == 1.0f);
  CHECK(manager.effectiveScale() == Catch::Approx(1.0f));
  CHECK(lastReloadScale == Catch::Approx(1.0f));

  manager.setUserScaleOverride(0.1f);
  CHECK(manager.userScaleOverride() == 0.5f);
  CHECK(manager.effectiveScale() == Catch::Approx(0.5f));

  manager.setUserScaleOverride(10.0f);
  CHECK(manager.userScaleOverride() == 4.0f);
  CHECK(manager.effectiveScale() == Catch::Approx(4.0f));

  manager.setUserScaleOverride(std::numeric_limits<float>::infinity());
  CHECK(manager.userScaleOverride() == 1.0f);

  manager.setUserScaleOverride(std::nullopt);
  (void)manager.applyContentScale(-3.0f);
  CHECK(manager.contentScale() == Catch::Approx(1.0f));
}

TEST_CASE("UI scale manager ignores unchanged user overrides", "[ui][scale]")
{
  ImGuiContextScope context;
  UiScaleManager manager;
  int reloadCount = 0;

  manager.captureBaseStyle(ImGui::GetStyle());
  manager.setFontReloadCallback([&reloadCount](float) { ++reloadCount; });

  manager.setUserScaleOverride(1.25f);
  manager.setUserScaleOverride(1.25f);

  CHECK(manager.userScaleOverride() == 1.25f);
  CHECK(reloadCount == 1);
}

TEST_CASE("UI scale manager can update the unscaled base style", "[ui][scale]")
{
  ImGuiContextScope context;
  UiScaleManager manager;

  ImGuiStyle baseStyle = ImGui::GetStyle();
  baseStyle.FramePadding = ImVec2{2.0f, 3.0f};
  manager.captureBaseStyle(baseStyle);

  manager.setUserScaleOverride(2.0f);
  manager.updateBaseStyle([](ImGuiStyle& style) { style.FramePadding = ImVec2{7.0f, 11.0f}; });

  CHECK(ImGui::GetStyle().FramePadding.x == Catch::Approx(14.0f));
  CHECK(ImGui::GetStyle().FramePadding.y == Catch::Approx(22.0f));
  CHECK(ImGui::GetStyle().FontScaleDpi == Catch::Approx(2.0f));

  manager.updateBaseStyle(nullptr);
  CHECK(ImGui::GetStyle().FramePadding.x == Catch::Approx(14.0f));
}

TEST_CASE("UI scale manager can explicitly rebuild fonts for the current scale", "[ui][scale]")
{
  ImGuiContextScope context;
  UiScaleManager manager;
  float lastReloadScale = 0.0f;
  int reloadCount = 0;

  manager.captureBaseStyle(ImGui::GetStyle());
  manager.setFontReloadCallback([&](float scale) {
    lastReloadScale = scale;
    ++reloadCount;
  });

  manager.setUserScaleOverride(1.75f);
  manager.rebuildFontsForCurrentScale();

  CHECK(reloadCount == 2);
  CHECK(lastReloadScale == Catch::Approx(1.75f));
}
