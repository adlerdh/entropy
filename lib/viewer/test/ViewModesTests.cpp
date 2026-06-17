#include "viewer/ViewModes.h"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <vector>

TEST_CASE("2D render mode choices stay in stable UI order", "[viewer][modes]")
{
  const std::vector expected{
    ViewRenderMode::Image,
    ViewRenderMode::Checkerboard,
    ViewRenderMode::Quadrants,
    ViewRenderMode::Flashlight,
    ViewRenderMode::Overlay,
    ViewRenderMode::Difference,
    ViewRenderMode::JointHistogram,
    ViewRenderMode::Disabled};

  CHECK(All2dViewRenderModes == expected);
  CHECK(All2dNonMetricRenderModes == std::vector{ViewRenderMode::Image, ViewRenderMode::Disabled});
}

TEST_CASE("3D render mode choices stay in stable UI order", "[viewer][modes]")
{
  const std::vector expected{ViewRenderMode::VolumeRender, ViewRenderMode::Disabled};

  CHECK(All3dViewRenderModes == expected);
  CHECK(All3dNonMetricRenderModes == expected);
}

TEST_CASE("view render mode enum ordinals remain stable for serialized layout specs", "[viewer][modes]")
{
  CHECK(static_cast<int>(ViewRenderMode::Image) == 0);
  CHECK(static_cast<int>(ViewRenderMode::Checkerboard) == 1);
  CHECK(static_cast<int>(ViewRenderMode::Quadrants) == 2);
  CHECK(static_cast<int>(ViewRenderMode::Flashlight) == 3);
  CHECK(static_cast<int>(ViewRenderMode::Overlay) == 4);
  CHECK(static_cast<int>(ViewRenderMode::Difference) == 5);
  CHECK(static_cast<int>(ViewRenderMode::JointHistogram) == 6);
  CHECK(static_cast<int>(ViewRenderMode::VolumeRender) == 7);
  CHECK(static_cast<int>(ViewRenderMode::Disabled) == 8);
  CHECK(static_cast<int>(ViewRenderMode::NumElements) == 9);
}

TEST_CASE("intensity projection modes stay in stable UI order", "[viewer][modes]")
{
  constexpr std::array expected{
    IntensityProjectionMode::None,
    IntensityProjectionMode::Maximum,
    IntensityProjectionMode::Mean,
    IntensityProjectionMode::Minimum,
    IntensityProjectionMode::Xray};

  CHECK(AllIntensityProjectionModes == expected);
}

TEST_CASE("intensity projection enum ordinals remain stable for serialized layout specs", "[viewer][modes]")
{
  CHECK(static_cast<int>(IntensityProjectionMode::None) == 0);
  CHECK(static_cast<int>(IntensityProjectionMode::Maximum) == 1);
  CHECK(static_cast<int>(IntensityProjectionMode::Mean) == 2);
  CHECK(static_cast<int>(IntensityProjectionMode::Minimum) == 3);
  CHECK(static_cast<int>(IntensityProjectionMode::Xray) == 4);
  CHECK(static_cast<int>(IntensityProjectionMode::NumElements) == 5);
}

TEST_CASE("viewer mode labels cover all public choices", "[viewer][modes]")
{
  for (const ViewRenderMode mode : All2dViewRenderModes) {
    CHECK_FALSE(typeString(mode).empty());
    CHECK_FALSE(descriptionString(mode).empty());
  }

  for (const ViewRenderMode mode : All3dViewRenderModes) {
    CHECK_FALSE(typeString(mode).empty());
    CHECK_FALSE(descriptionString(mode).empty());
  }

  for (const IntensityProjectionMode mode : AllIntensityProjectionModes) {
    CHECK_FALSE(typeString(mode).empty());
    CHECK_FALSE(descriptionString(mode).empty());
  }

  CHECK(typeString(ViewRenderMode::Image) == "Layers");
  CHECK(typeString(IntensityProjectionMode::Xray) == "X-ray Projection");
}

TEST_CASE("viewer mode labels tolerate sentinel values", "[viewer][modes]")
{
  CHECK(typeString(ViewRenderMode::NumElements) == "Unknown");
  CHECK(descriptionString(ViewRenderMode::NumElements) == "Unknown render mode");
  CHECK(typeString(static_cast<ViewRenderMode>(100)) == "Unknown");
  CHECK(descriptionString(static_cast<ViewRenderMode>(100)) == "Unknown render mode");

  CHECK(typeString(IntensityProjectionMode::NumElements) == "Unknown Projection");
  CHECK(descriptionString(IntensityProjectionMode::NumElements) == "Unknown intensity projection");
  CHECK(typeString(static_cast<IntensityProjectionMode>(100)) == "Unknown Projection");
  CHECK(descriptionString(static_cast<IntensityProjectionMode>(100)) == "Unknown intensity projection");
}
