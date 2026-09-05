#include "viewer/ThreeDSceneContents.h"
#include "viewer/ViewModes.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <ranges>
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
    ViewRenderMode::LocalNcc,
    ViewRenderMode::LocalLinearResidual,
    ViewRenderMode::Disabled};

  CHECK(All2dViewRenderModes == expected);
  CHECK(All2dSingleImageRenderModes == std::vector{ViewRenderMode::Image, ViewRenderMode::Disabled});
}

TEST_CASE("comparison render modes require at least two images", "[viewer][modes]")
{
  const std::array comparisonModes{
    ViewRenderMode::Checkerboard,
    ViewRenderMode::Quadrants,
    ViewRenderMode::Flashlight,
    ViewRenderMode::Overlay,
    ViewRenderMode::Difference,
    ViewRenderMode::JointHistogram,
    ViewRenderMode::LocalNcc,
    ViewRenderMode::LocalLinearResidual};

  for (const ViewRenderMode mode : comparisonModes) {
    CHECK(isComparisonRenderMode(mode));
    CHECK(reconcileRenderMode(mode, 0) == ViewRenderMode::Image);
    CHECK(reconcileRenderMode(mode, 1) == ViewRenderMode::Image);
    CHECK(reconcileRenderMode(mode, 2) == mode);
  }

  CHECK(std::ranges::none_of(twoDRenderModesForImageCount(0), isComparisonRenderMode));
  CHECK(std::ranges::none_of(twoDRenderModesForImageCount(1), isComparisonRenderMode));
  CHECK(std::ranges::all_of(
    twoDRenderModesForImageCount(2) | std::views::drop(1) | std::views::take(7),
    isComparisonRenderMode));
}

TEST_CASE("3D scene contents are independent and extensible", "[viewer][three_d_scene]")
{
  ThreeDSceneContents contents;
  CHECK(contents.empty());

  contents.insert(ThreeDSceneContent::Segmentations);
  CHECK(contents.contains(ThreeDSceneContent::Segmentations));
  CHECK_FALSE(contents.contains(ThreeDSceneContent::Isosurfaces));

  contents.insert(ThreeDSceneContent::Isosurfaces);
  CHECK(contents == DefaultThreeDSceneContents);

  contents.erase(ThreeDSceneContent::Segmentations);
  CHECK_FALSE(contents.contains(ThreeDSceneContent::Segmentations));
  CHECK(contents.contains(ThreeDSceneContent::Isosurfaces));
}

TEST_CASE("view render modes use compact 2D-only values", "[viewer][modes]")
{
  CHECK(static_cast<int>(ViewRenderMode::Image) == 0);
  CHECK(static_cast<int>(ViewRenderMode::Checkerboard) == 1);
  CHECK(static_cast<int>(ViewRenderMode::Quadrants) == 2);
  CHECK(static_cast<int>(ViewRenderMode::Flashlight) == 3);
  CHECK(static_cast<int>(ViewRenderMode::Overlay) == 4);
  CHECK(static_cast<int>(ViewRenderMode::Difference) == 5);
  CHECK(static_cast<int>(ViewRenderMode::JointHistogram) == 6);
  CHECK(static_cast<int>(ViewRenderMode::LocalNcc) == 7);
  CHECK(static_cast<int>(ViewRenderMode::LocalLinearResidual) == 8);
  CHECK(static_cast<int>(ViewRenderMode::Disabled) == 9);
  CHECK(static_cast<int>(ViewRenderMode::NumElements) == 10);
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

  for (const IntensityProjectionMode mode : AllIntensityProjectionModes) {
    CHECK_FALSE(typeString(mode).empty());
    CHECK_FALSE(descriptionString(mode).empty());
  }

  CHECK(typeString(ViewRenderMode::Image) == "Layers");
  CHECK(typeString(IntensityProjectionMode::Xray) == "X-ray projection");
}

TEST_CASE("viewer mode labels tolerate sentinel values", "[viewer][modes]")
{
  CHECK(typeString(ViewRenderMode::NumElements) == "Unknown");
  CHECK(descriptionString(ViewRenderMode::NumElements) == "Unknown render mode");
  CHECK(typeString(static_cast<ViewRenderMode>(100)) == "Unknown");
  CHECK(descriptionString(static_cast<ViewRenderMode>(100)) == "Unknown render mode");

  CHECK(typeString(IntensityProjectionMode::NumElements) == "Unknown projection");
  CHECK(descriptionString(IntensityProjectionMode::NumElements) == "Unknown intensity projection");
  CHECK(typeString(static_cast<IntensityProjectionMode>(100)) == "Unknown projection");
  CHECK(descriptionString(static_cast<IntensityProjectionMode>(100)) == "Unknown intensity projection");
}
