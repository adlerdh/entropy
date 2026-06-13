#include "ui/UiScaleManager.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

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
