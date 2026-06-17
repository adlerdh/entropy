#include "viewer/LayoutTypes.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("layout kind enum ordinals remain stable for serialized layout specs", "[viewer][layout]")
{
  CHECK(static_cast<int>(LayoutKind::Custom) == 0);
  CHECK(static_cast<int>(LayoutKind::FourUp) == 1);
  CHECK(static_cast<int>(LayoutKind::Tri) == 2);
  CHECK(static_cast<int>(LayoutKind::SingleAxial) == 3);
  CHECK(static_cast<int>(LayoutKind::MultiImageAxialGrid) == 4);
  CHECK(static_cast<int>(LayoutKind::AxCorSagByImage) == 5);
  CHECK(static_cast<int>(LayoutKind::AxialLightbox) == 6);
  CHECK(static_cast<int>(LayoutKind::CoronalLightbox) == 7);
  CHECK(static_cast<int>(LayoutKind::SagittalLightbox) == 8);
  CHECK(static_cast<int>(LayoutKind::SingleCoronal) == 9);
  CHECK(static_cast<int>(LayoutKind::SingleSagittal) == 10);
  CHECK(static_cast<int>(LayoutKind::MultiImageCoronalGrid) == 11);
  CHECK(static_cast<int>(LayoutKind::MultiImageSagittalGrid) == 12);
  CHECK(static_cast<int>(LayoutKind::NumElements) == 13);
}

TEST_CASE("camera sync mode enum ordinals remain stable for layout sync specs", "[viewer][layout]")
{
  CHECK(static_cast<int>(CameraSyncMode::Rotation) == 0);
  CHECK(static_cast<int>(CameraSyncMode::Translation) == 1);
  CHECK(static_cast<int>(CameraSyncMode::Zoom) == 2);
}
