#include "rendering/geometry/PixelEdgeGeometry.h"

#include <catch2/catch_test_macros.hpp>

namespace pixel_edge = rendering::pixel_edge;

namespace
{

void requireRect(
  const pixel_edge::ViewRect& rect,
  int sceneX,
  int sceneY,
  int windowX,
  int windowY,
  int width,
  int height)
{
  REQUIRE(rect.sceneX == sceneX);
  REQUIRE(rect.sceneY == sceneY);
  REQUIRE(rect.windowX == windowX);
  REQUIRE(rect.windowY == windowY);
  REQUIRE(rect.width == width);
  REQUIRE(rect.height == height);
}

} // namespace

TEST_CASE("pixel edge view rectangle covers full render viewport", "[rendering][pixel-edge]")
{
  const auto rect =
    pixel_edge::computeViewRect(glm::vec4{-1.0f, -1.0f, 2.0f, 2.0f}, glm::vec4{0.0f, 0.0f, 800.0f, 600.0f});

  requireRect(rect, 0, 0, 0, 0, 800, 600);
}

TEST_CASE("pixel edge view rectangle converts clip-space quadrants to pixels", "[rendering][pixel-edge]")
{
  const auto rect =
    pixel_edge::computeViewRect(glm::vec4{0.0f, 0.0f, 1.0f, 1.0f}, glm::vec4{0.0f, 0.0f, 800.0f, 600.0f});

  requireRect(rect, 400, 300, 400, 300, 400, 300);
}

TEST_CASE("pixel edge view rectangle preserves default framebuffer origin", "[rendering][pixel-edge]")
{
  const auto rect =
    pixel_edge::computeViewRect(glm::vec4{-1.0f, 0.0f, 1.0f, 1.0f}, glm::vec4{24.0f, 48.0f, 1200.0f, 900.0f});

  requireRect(rect, 0, 450, 24, 498, 600, 450);
}
