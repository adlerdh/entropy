#include "rendering/ascii/AsciiClipboard.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

namespace ascii_clipboard = entropy::rendering::ascii_clipboard;

TEST_CASE("ASCII clipboard RGB conversion clamps and rounds", "[rendering][ascii][clipboard]")
{
  CHECK(ascii_clipboard::toRgb8({-1.0f, 0.5f, 2.0f}) == entropy::clipboard::Rgb8{0, 128, 255});
  CHECK(
    ascii_clipboard::toRgb8({1.0f / 255.0f, 15.0f / 255.0f, 127.0f / 255.0f}) == entropy::clipboard::Rgb8{1, 15, 127});
}

TEST_CASE("ASCII clipboard fixed foreground mode uses configured color", "[rendering][ascii][clipboard]")
{
  const glm::vec3 color = ascii_clipboard::foregroundColorForCell({0.1f, 0.8f, 0.3f}, false, {0.25f, 0.5f, 0.75f});

  CHECK(color.r == Catch::Approx(0.25f));
  CHECK(color.g == Catch::Approx(0.5f));
  CHECK(color.b == Catch::Approx(0.75f));
}

TEST_CASE("ASCII clipboard colormap mode preserves hue saturation and boosts value", "[rendering][ascii][clipboard]")
{
  const glm::vec3 color = ascii_clipboard::foregroundColorForCell({0.2f, 0.4f, 0.1f}, true, {1.0f, 1.0f, 1.0f});

  CHECK(color.r == Catch::Approx(0.5f).margin(1.0e-5f));
  CHECK(color.g == Catch::Approx(1.0f).margin(1.0e-5f));
  CHECK(color.b == Catch::Approx(0.25f).margin(1.0e-5f));
}

TEST_CASE("ASCII clipboard span opens with CSS hex color", "[rendering][ascii][clipboard]")
{
  CHECK(ascii_clipboard::colorSpanOpen({0, 128, 255}) == "<span style=\"color:#0080FF\">");
}
