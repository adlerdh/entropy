#include "rendering/helpers/VectorDrawingHelpers.h"

#include "common/Viewport.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <limits>

namespace vector_drawing = rendering::vector_drawing;

TEST_CASE("vector drawing helpers classify finite positions and rectangles", "[rendering][vector-drawing]")
{
  REQUIRE(vector_drawing::isFiniteVec2(glm::vec2{1.0f, -2.0f}));
  REQUIRE_FALSE(vector_drawing::isFiniteVec2(glm::vec2{std::numeric_limits<float>::infinity(), 0.0f}));
  REQUIRE_FALSE(vector_drawing::isFiniteVec2(glm::vec2{0.0f, std::numeric_limits<float>::quiet_NaN()}));

  REQUIRE(vector_drawing::isInsideRect(glm::vec2{2.0f, 3.0f}, glm::vec2{1.0f, 1.0f}, glm::vec2{4.0f, 4.0f}));
  REQUIRE_FALSE(vector_drawing::isInsideRect(glm::vec2{5.0f, 3.0f}, glm::vec2{1.0f, 1.0f}, glm::vec2{4.0f, 4.0f}));
}

TEST_CASE("vector drawing helpers compute arrow geometry", "[rendering][vector-drawing]")
{
  REQUIRE_FALSE(vector_drawing::computeArrowGeometry(glm::vec2{0.0f}, glm::vec2{1.0f, 0.0f}, 1.0f));

  const auto arrow = vector_drawing::computeArrowGeometry(glm::vec2{0.0f, 0.0f}, glm::vec2{20.0f, 0.0f}, 2.0f);
  REQUIRE(arrow);
  REQUIRE(arrow->shaftStart.x == Catch::Approx(0.0f));
  REQUIRE(arrow->shaftStart.y == Catch::Approx(0.0f));
  REQUIRE(arrow->shaftEnd.x == Catch::Approx(19.0f));
  REQUIRE(arrow->shaftEnd.y == Catch::Approx(0.0f));
  REQUIRE(arrow->headTip.x == Catch::Approx(20.0f));
  REQUIRE(arrow->headTip.y == Catch::Approx(0.0f));
  REQUIRE(arrow->headLeft.x == Catch::Approx(13.6f));
  REQUIRE(arrow->headRight.x == Catch::Approx(13.6f));
  REQUIRE(arrow->headLeft.y == Catch::Approx(-arrow->headRight.y));
}

TEST_CASE("vector drawing checker coordinates preserve apparent square cells", "[rendering][vector-drawing]")
{
  const glm::vec2 wide = vector_drawing::checkerCoordForViewClip(glm::vec2{0.0f, 0.0f}, 8.0f, 2.0f);
  REQUIRE(wide.x == Catch::Approx(4.0f));
  REQUIRE(wide.y == Catch::Approx(2.0f));

  const glm::vec2 tall = vector_drawing::checkerCoordForViewClip(glm::vec2{0.0f, 0.0f}, 8.0f, 0.5f);
  REQUIRE(tall.x == Catch::Approx(2.0f));
  REQUIRE(tall.y == Catch::Approx(4.0f));
}

TEST_CASE("vector drawing comparison sample helper covers image and checkerboard modes", "[rendering][vector-drawing]")
{
  REQUIRE(vector_drawing::shouldRenderFixedComparisonSample(
    ViewRenderMode::Image,
    glm::vec2{0.0f},
    glm::vec2{0.0f},
    glm::vec2{0.0f},
    glm::ivec2{1, 1},
    false,
    1.0f,
    0.25f,
    false));

  REQUIRE(vector_drawing::shouldRenderFixedComparisonSample(
    ViewRenderMode::Checkerboard,
    glm::vec2{0.0f},
    glm::vec2{1.2f, 0.2f},
    glm::vec2{0.0f},
    glm::ivec2{1, 1},
    true,
    1.0f,
    0.25f,
    false));

  REQUIRE_FALSE(vector_drawing::shouldRenderFixedComparisonSample(
    ViewRenderMode::Checkerboard,
    glm::vec2{0.0f},
    glm::vec2{1.2f, 0.2f},
    glm::vec2{0.0f},
    glm::ivec2{1, 1},
    false,
    1.0f,
    0.25f,
    false));
}

TEST_CASE("vector drawing comparison sample helper covers quadrants and flashlight", "[rendering][vector-drawing]")
{
  REQUIRE(vector_drawing::shouldRenderFixedComparisonSample(
    ViewRenderMode::Quadrants,
    glm::vec2{-0.5f, 0.5f},
    glm::vec2{0.0f},
    glm::vec2{0.0f},
    glm::ivec2{1, 1},
    true,
    1.0f,
    0.25f,
    false));

  REQUIRE(vector_drawing::shouldRenderFixedComparisonSample(
    ViewRenderMode::Flashlight,
    glm::vec2{0.0f, 0.0f},
    glm::vec2{0.0f},
    glm::vec2{0.0f},
    glm::ivec2{1, 1},
    false,
    1.0f,
    0.25f,
    false));

  REQUIRE(vector_drawing::shouldRenderFixedComparisonSample(
    ViewRenderMode::Flashlight,
    glm::vec2{0.0f, 0.0f},
    glm::vec2{0.0f},
    glm::vec2{0.0f},
    glm::ivec2{1, 1},
    true,
    1.0f,
    0.25f,
    true));
}

TEST_CASE("vector drawing helpers convert between miewport and clip coordinates", "[rendering][vector-drawing]")
{
  const Viewport windowViewport{0.0f, 0.0f, 200.0f, 100.0f};

  const glm::vec2 viewClip =
    vector_drawing::viewClipFromMiewport(windowViewport, glm::mat4{1.0f}, glm::vec2{100.0f, 25.0f});
  REQUIRE(viewClip.x == Catch::Approx(0.0f));
  REQUIRE(viewClip.y == Catch::Approx(0.5f));

  const glm::vec2 miewport =
    vector_drawing::projectWorldToMiewport(windowViewport, glm::mat4{1.0f}, glm::vec3{0.0f, 0.5f, 0.0f});
  REQUIRE(miewport.x == Catch::Approx(100.0f));
  REQUIRE(miewport.y == Catch::Approx(25.0f));
}

TEST_CASE("vector drawing helpers compute screen scales and spacing", "[rendering][vector-drawing]")
{
  REQUIRE(
    vector_drawing::screenDistanceFromMiewportPositions(glm::vec2{1.0f, 2.0f}, glm::vec2{4.0f, 6.0f}) ==
    Catch::Approx(5.0f));
  REQUIRE(
    vector_drawing::screenDistanceFromMiewportPositions(
      glm::vec2{std::numeric_limits<float>::infinity(), 0.0f},
      glm::vec2{4.0f, 6.0f}) == Catch::Approx(0.0f));

  REQUIRE(vector_drawing::meanScreenPixelsPerMillimeter(4.0f, 6.0f) == Catch::Approx(5.0f));
  REQUIRE(vector_drawing::meanScreenPixelsPerMillimeter(0.0f, 0.0f) == Catch::Approx(1.0f));
  REQUIRE(vector_drawing::meanInPlaneScreenPixelsPerVoxel({2.0f, 8.0f, 4.0f}) == Catch::Approx(6.0f));
  REQUIRE(vector_drawing::meanInPlaneScreenPixelsPerVoxel({0.0f, 0.0f, 0.0f}) == Catch::Approx(1.0f));

  REQUIRE(
    vector_drawing::vectorArrowSpacingPixels(VectorArrowOverlaySpacingMode::Pixels, 12.0f, 2.0f, 3.0f, 4.0f, 5.0f) ==
    Catch::Approx(12.0f));
  REQUIRE(
    vector_drawing::vectorArrowSpacingPixels(VectorArrowOverlaySpacingMode::Voxels, 12.0f, 2.0f, 3.0f, 4.0f, 5.0f) ==
    Catch::Approx(8.0f));
  REQUIRE(
    vector_drawing::vectorArrowSpacingPixels(
      VectorArrowOverlaySpacingMode::Millimeters,
      12.0f,
      2.0f,
      3.0f,
      4.0f,
      5.0f) == Catch::Approx(15.0f));
}
