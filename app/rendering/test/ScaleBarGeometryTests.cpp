#include "rendering/ScaleBarGeometry.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <glm/vec4.hpp>

namespace scale_bar = entropy::rendering::scale_bar;

namespace
{
FrameBounds frame(float x, float y, float width, float height)
{
  return FrameBounds{glm::vec4{x, y, width, height}};
}

void requireVec2(const glm::vec2& value, float x, float y)
{
  REQUIRE(value.x == Catch::Approx(x));
  REQUIRE(value.y == Catch::Approx(y));
}
} // namespace

TEST_CASE("scale bar nice length stays within drawable bounds", "[rendering][scale-bar]")
{
  REQUIRE(scale_bar::computeNiceScaleBarLengthMm(83.0, 28.0, 200.0) == Catch::Approx(50.0));
  REQUIRE(scale_bar::computeNiceScaleBarLengthMm(31.0, 28.0, 200.0) == Catch::Approx(50.0));

  // Regression: old rounding could choose 20 mm here, causing a sub-minimum 20 px bar to disappear.
  REQUIRE(scale_bar::computeNiceScaleBarLengthMm(29.0, 28.0, 200.0) == Catch::Approx(50.0));

  REQUIRE(scale_bar::computeNiceScaleBarLengthMm(20.0, 28.0, 200.0) == Catch::Approx(50.0));
  REQUIRE(scale_bar::computeNiceScaleBarLengthMm(20.0, 200.0, 28.0) == Catch::Approx(0.0));
  REQUIRE(scale_bar::computeNiceScaleBarLengthMm(0.0, 28.0, 200.0) == Catch::Approx(0.0));
}

TEST_CASE("scale bar length formatting chooses metric units", "[rendering][scale-bar]")
{
  REQUIRE(scale_bar::formatScaleBarLength(2.0) == "2 mm");
  REQUIRE(scale_bar::formatScaleBarLength(25.0) == "2.5 cm");
  REQUIRE(scale_bar::formatScaleBarLength(1000.0) == "1 m");
  REQUIRE(scale_bar::formatScaleBarLength(1.0e6) == "1 km");
  REQUIRE(scale_bar::formatScaleBarLength(0.25) == "250 µm");
  REQUIRE(scale_bar::formatScaleBarLength(0.00025) == "250 nm");
}

TEST_CASE("scale bar automatic ticks preserve readable spacing", "[rendering][scale-bar]")
{
  REQUIRE(scale_bar::computeScaleBarIntervals(140.0f, ScaleBarTicks::Automatic) == 5);
  REQUIRE(scale_bar::computeScaleBarIntervals(100.0f, ScaleBarTicks::Automatic) == 4);
  REQUIRE(scale_bar::computeScaleBarIntervals(60.0f, ScaleBarTicks::Automatic) == 2);
  REQUIRE(scale_bar::computeScaleBarIntervals(40.0f, ScaleBarTicks::Automatic) == 1);
  REQUIRE(scale_bar::computeScaleBarIntervals(140.0f, ScaleBarTicks::Endpoints) == 1);
}

TEST_CASE("scale bar orientation helpers return drawing axes", "[rendering][scale-bar]")
{
  requireVec2(scale_bar::direction(ScaleBarOrientation::Horizontal), 1.0f, 0.0f);
  requireVec2(scale_bar::direction(ScaleBarOrientation::Vertical), 0.0f, -1.0f);
  requireVec2(scale_bar::tickDirection(ScaleBarOrientation::Horizontal), 0.0f, 1.0f);
  requireVec2(scale_bar::tickDirection(ScaleBarOrientation::Vertical), 1.0f, 0.0f);
}

TEST_CASE("scale bar position helpers classify edge and corner positions", "[rendering][scale-bar]")
{
  REQUIRE(scale_bar::isBottom(ScaleBarPosition::BottomRight));
  REQUIRE(scale_bar::isBottom(ScaleBarPosition::BottomLeft));
  REQUIRE(scale_bar::isBottom(ScaleBarPosition::Bottom));
  REQUIRE_FALSE(scale_bar::isBottom(ScaleBarPosition::Top));

  REQUIRE(scale_bar::isTop(ScaleBarPosition::TopRight));
  REQUIRE(scale_bar::isTop(ScaleBarPosition::TopLeft));
  REQUIRE(scale_bar::isTop(ScaleBarPosition::Top));
  REQUIRE_FALSE(scale_bar::isTop(ScaleBarPosition::Bottom));

  REQUIRE(scale_bar::isLeft(ScaleBarPosition::TopLeft));
  REQUIRE(scale_bar::isLeft(ScaleBarPosition::BottomLeft));
  REQUIRE(scale_bar::isLeft(ScaleBarPosition::Left));
  REQUIRE_FALSE(scale_bar::isLeft(ScaleBarPosition::Right));

  REQUIRE(scale_bar::isRight(ScaleBarPosition::TopRight));
  REQUIRE(scale_bar::isRight(ScaleBarPosition::BottomRight));
  REQUIRE(scale_bar::isRight(ScaleBarPosition::Right));
  REQUIRE_FALSE(scale_bar::isRight(ScaleBarPosition::Left));
}

TEST_CASE("scale bar layout computes horizontal bottom-right geometry", "[rendering][scale-bar]")
{
  const auto layout = scale_bar::computeLayout(
    frame(10.0f, 20.0f, 500.0f, 300.0f),
    glm::vec2{1.0f, 1.0f},
    ScaleBarPosition::BottomRight,
    ScaleBarOrientation::Horizontal,
    ScaleBarTicks::Automatic,
    0.2f,
    12.0f);

  REQUIRE(layout);
  REQUIRE(layout->lengthMm == Catch::Approx(50.0));
  REQUIRE(layout->barLengthPx == Catch::Approx(50.0f));
  REQUIRE(layout->intervals == 2);
  requireVec2(layout->start, 448.0f, 303.5f);
  requireVec2(layout->end, 498.0f, 303.5f);
  requireVec2(layout->tickAxis, 0.0f, 1.0f);
  REQUIRE(layout->label == "5 cm");
  REQUIRE(layout->labelPos.x == Catch::Approx(473.0f));
  REQUIRE(layout->labelPos.y < layout->start.y);
}

TEST_CASE("scale bar layout computes vertical left geometry", "[rendering][scale-bar]")
{
  const auto layout = scale_bar::computeLayout(
    frame(10.0f, 20.0f, 500.0f, 300.0f),
    glm::vec2{1.0f, 2.0f},
    ScaleBarPosition::Left,
    ScaleBarOrientation::Vertical,
    ScaleBarTicks::Automatic,
    0.2f,
    12.0f);

  REQUIRE(layout);
  REQUIRE(layout->lengthMm == Catch::Approx(100.0));
  REQUIRE(layout->barLengthPx == Catch::Approx(50.0f));
  REQUIRE(layout->barLengthPx >= 28.0f);
  requireVec2(layout->start, 26.5f, 195.0f);
  requireVec2(layout->end, 26.5f, 145.0f);
  requireVec2(layout->tickAxis, 1.0f, 0.0f);
  REQUIRE(layout->label == "10 cm");
  REQUIRE(layout->labelPos.x > layout->start.x);
}

TEST_CASE(
  "scale bar layout does not disappear when the first rounded-down length is too short",
  "[rendering][scale-bar]")
{
  const auto layout = scale_bar::computeLayout(
    frame(0.0f, 0.0f, 524.0f, 300.0f),
    glm::vec2{1.0f, 1.0f},
    ScaleBarPosition::BottomRight,
    ScaleBarOrientation::Horizontal,
    ScaleBarTicks::Automatic,
    0.05f,
    12.0f);

  REQUIRE(layout);
  REQUIRE(layout->barLengthPx >= 28.0f);
  REQUIRE(layout->barLengthPx <= 500.0f);
  REQUIRE(layout->lengthMm == Catch::Approx(50.0));
}

TEST_CASE("scale bar layout rejects views without a readable scale bar", "[rendering][scale-bar]")
{
  REQUIRE_FALSE(scale_bar::computeLayout(
    frame(0.0f, 0.0f, 40.0f, 300.0f),
    glm::vec2{1.0f, 1.0f},
    ScaleBarPosition::BottomRight,
    ScaleBarOrientation::Horizontal,
    ScaleBarTicks::Automatic,
    0.2f,
    12.0f));

  REQUIRE_FALSE(scale_bar::computeLayout(
    frame(0.0f, 0.0f, 500.0f, 300.0f),
    glm::vec2{0.0f, 1.0f},
    ScaleBarPosition::BottomRight,
    ScaleBarOrientation::Horizontal,
    ScaleBarTicks::Automatic,
    0.2f,
    12.0f));
}
