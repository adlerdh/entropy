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
    CHECK(reconcileRenderMode(ViewType::Axial, mode, 0) == ViewRenderMode::Image);
    CHECK(reconcileRenderMode(ViewType::Coronal, mode, 1) == ViewRenderMode::Image);
  }

  CHECK(reconcileRenderMode(ViewType::Sagittal, ViewRenderMode::Difference, 2) == ViewRenderMode::Difference);
  CHECK(reconcileRenderMode(ViewType::Sagittal, ViewRenderMode::LocalNcc, 2) == ViewRenderMode::LocalNcc);
  CHECK(reconcileRenderMode(ViewType::Sagittal, ViewRenderMode::JointHistogram, 2) == ViewRenderMode::Image);
  CHECK_FALSE(isRenderModeCompatibleWithViewType(ViewType::Axial, ViewRenderMode::NumElements));

  CHECK(std::ranges::none_of(twoDRenderModesForImageCount(0), isComparisonRenderMode));
  CHECK(std::ranges::none_of(twoDRenderModesForImageCount(1), isComparisonRenderMode));
  CHECK(std::ranges::all_of(
    twoDRenderModesForImageCount(2) | std::views::drop(1) | std::views::take(7),
    isComparisonRenderMode));
}

TEST_CASE("image-count reconciliation clears cached comparison modes", "[viewer][modes]")
{
  const ViewRenderModeState twoDState{
    .current = ViewRenderMode::Difference,
    .last2d = ViewRenderMode::Difference,
    .last3d = ViewRenderMode::SegmentationAndIsosurfaces};
  CHECK(
    reconcileRenderModeState(ViewType::Axial, twoDState, 1) == ViewRenderModeState{
                                                                 .current = ViewRenderMode::Image,
                                                                 .last2d = ViewRenderMode::Image,
                                                                 .last3d = ViewRenderMode::SegmentationAndIsosurfaces});

  const ViewRenderModeState threeDState{
    .current = ViewRenderMode::SegmentationMesh,
    .last2d = ViewRenderMode::LocalNcc,
    .last3d = ViewRenderMode::SegmentationMesh};
  CHECK(
    reconcileRenderModeState(ViewType::ThreeD, threeDState, 1) == ViewRenderModeState{
                                                                    .current = ViewRenderMode::SegmentationMesh,
                                                                    .last2d = ViewRenderMode::Image,
                                                                    .last3d = ViewRenderMode::SegmentationMesh});
}

TEST_CASE("3D render mode choices stay in stable UI order", "[viewer][modes]")
{
  const std::vector expected{
    ViewRenderMode::SegmentationMesh,
    ViewRenderMode::Isosurfaces,
    ViewRenderMode::SegmentationAndIsosurfaces,
    ViewRenderMode::Disabled};

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
  CHECK(static_cast<int>(ViewRenderMode::Isosurfaces) == 7);
  CHECK(static_cast<int>(ViewRenderMode::Disabled) == 8);
  CHECK(static_cast<int>(ViewRenderMode::LocalNcc) == 9);
  CHECK(static_cast<int>(ViewRenderMode::LocalLinearResidual) == 10);
  CHECK(static_cast<int>(ViewRenderMode::SegmentationMesh) == 11);
  CHECK(static_cast<int>(ViewRenderMode::SegmentationAndIsosurfaces) == 12);
  CHECK(static_cast<int>(ViewRenderMode::NumElements) == 13);
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
  CHECK(typeString(ViewRenderMode::SegmentationMesh) == "Segmentations");
  CHECK(typeString(ViewRenderMode::Isosurfaces) == "Isosurfaces");
  CHECK(typeString(ViewRenderMode::SegmentationAndIsosurfaces) == "Both (Seg + Iso)");
  CHECK(
    descriptionString(ViewRenderMode::SegmentationMesh) == "Render visible segmentation labels as 3D surface meshes");
  CHECK(descriptionString(ViewRenderMode::Isosurfaces) == "Render visible image isosurfaces in 3D");
  CHECK(
    descriptionString(ViewRenderMode::SegmentationAndIsosurfaces) ==
    "Render visible segmentation meshes and image isosurfaces together");
  CHECK(typeString(IntensityProjectionMode::Xray) == "X-ray projection");
}

TEST_CASE("3D surface mode capabilities distinguish individual and combined rendering", "[viewer][modes]")
{
  CHECK(rendersSegmentations(ViewRenderMode::SegmentationMesh));
  CHECK_FALSE(rendersIsosurfaces(ViewRenderMode::SegmentationMesh));
  CHECK(rendersIsosurfaces(ViewRenderMode::Isosurfaces));
  CHECK_FALSE(rendersSegmentations(ViewRenderMode::Isosurfaces));
  CHECK(rendersSegmentations(ViewRenderMode::SegmentationAndIsosurfaces));
  CHECK(rendersIsosurfaces(ViewRenderMode::SegmentationAndIsosurfaces));
  CHECK(is3dRenderMode(ViewRenderMode::SegmentationAndIsosurfaces));
  CHECK_FALSE(is3dRenderMode(ViewRenderMode::Image));
}

TEST_CASE("3D render mode composes independent scene-content choices", "[viewer][modes]")
{
  CHECK(threeDRenderMode(false, false) == ViewRenderMode::Disabled);
  CHECK(threeDRenderMode(true, false) == ViewRenderMode::SegmentationMesh);
  CHECK(threeDRenderMode(false, true) == ViewRenderMode::Isosurfaces);
  CHECK(threeDRenderMode(true, true) == ViewRenderMode::SegmentationAndIsosurfaces);
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
