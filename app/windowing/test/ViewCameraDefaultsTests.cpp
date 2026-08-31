#include "windowing/ViewCameraDefaults.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("3D views initialize their hidden slice camera with 2D defaults", "[windowing][camera]")
{
  CHECK(windowing::initialSliceViewType(ViewType::ThreeD) == ViewType::Axial);
  CHECK(windowing::initialSliceProjectionType(ViewType::ThreeD) == ProjectionType::Orthographic);
}

TEST_CASE("2D views initialize their slice camera from their own view type", "[windowing][camera]")
{
  CHECK(windowing::initialSliceViewType(ViewType::Axial) == ViewType::Axial);
  CHECK(windowing::initialSliceViewType(ViewType::Coronal) == ViewType::Coronal);
  CHECK(windowing::initialSliceViewType(ViewType::Sagittal) == ViewType::Sagittal);
  CHECK(windowing::initialSliceViewType(ViewType::Oblique) == ViewType::Oblique);

  CHECK(windowing::initialSliceProjectionType(ViewType::Axial) == ProjectionType::Orthographic);
  CHECK(windowing::initialSliceProjectionType(ViewType::Coronal) == ProjectionType::Orthographic);
  CHECK(windowing::initialSliceProjectionType(ViewType::Sagittal) == ProjectionType::Orthographic);
  CHECK(windowing::initialSliceProjectionType(ViewType::Oblique) == ProjectionType::Orthographic);
}

TEST_CASE("orthogonal slice cameras track live crosshairs axes", "[windowing][camera][crosshairs]")
{
  CHECK(windowing::sliceCameraTracksCrosshairs(ViewType::Axial));
  CHECK(windowing::sliceCameraTracksCrosshairs(ViewType::Coronal));
  CHECK(windowing::sliceCameraTracksCrosshairs(ViewType::Sagittal));

  CHECK_FALSE(windowing::sliceCameraTracksCrosshairs(ViewType::Oblique));
  CHECK_FALSE(windowing::sliceCameraTracksCrosshairs(ViewType::ThreeD));
}
