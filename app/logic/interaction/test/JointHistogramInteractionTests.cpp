#include "logic/interaction/JointHistogramInteraction.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

TEST_CASE("joint histogram plot coordinates preserve the square plot geometry", "[interaction][joint_histogram]")
{
  const FrameBounds frame{{100.0f, 200.0f, 500.0f, 300.0f}};
  const joint_histogram::Plot plot = joint_histogram::plotForFrame(frame);

  CHECK(plot.size == Catch::Approx(214.0f));
  REQUIRE(joint_histogram::plotCoordinates(plot, frame, {-1.0f, 1.0f}));

  const glm::vec2 centerViewClip{
    2.0f * ((plot.left - frame.bounds.xoffset + 0.5f * plot.size) / frame.bounds.width) - 1.0f,
    1.0f - 2.0f * ((plot.top - frame.bounds.yoffset + 0.5f * plot.size) / frame.bounds.height)};
  const auto center = joint_histogram::plotCoordinates(plot, frame, centerViewClip);
  REQUIRE(center);
  CHECK(center->x == Catch::Approx(0.5f));
  CHECK(center->y == Catch::Approx(0.5f));
  CHECK(joint_histogram::contains(*center));
}

TEST_CASE("joint histogram plot clears the top-left view controls", "[interaction][joint_histogram]")
{
  const FrameBounds frame{{100.0f, 200.0f, 500.0f, 300.0f}};
  const joint_histogram::Plot plot = joint_histogram::plotForFrame(frame, 42.0f);

  CHECK(plot.top >= frame.bounds.yoffset + 50.0f);
  CHECK(plot.size == Catch::Approx(181.0f));
}

TEST_CASE(
  "joint histogram navigation zooms about an anchor and remains in its domain",
  "[interaction][joint_histogram]")
{
  joint_histogram::Navigation navigation;
  navigation.zoom(2.0f, {0.25f, 0.75f});

  CHECK(navigation.visibleMinimum().x == Catch::Approx(0.125f));
  CHECK(navigation.visibleMinimum().y == Catch::Approx(0.375f));
  CHECK(navigation.visibleMaximum().x == Catch::Approx(0.625f));
  CHECK(navigation.visibleMaximum().y == Catch::Approx(0.875f));

  navigation.pan({1.0f, -1.0f});
  CHECK(navigation.visibleMinimum().x == Catch::Approx(0.0f));
  CHECK(navigation.visibleMaximum().x == Catch::Approx(0.5f));
  CHECK(navigation.visibleMinimum().y == Catch::Approx(0.5f));
  CHECK(navigation.visibleMaximum().y == Catch::Approx(1.0f));

  navigation.reset();
  CHECK(navigation.visibleMinimum() == glm::vec2{0.0f});
  CHECK(navigation.visibleMaximum() == glm::vec2{1.0f});
}
