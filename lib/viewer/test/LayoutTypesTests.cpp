#include "viewer/LayoutTypes.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("layout kind enum ordinals remain stable for serialized layout specs", "[viewer][layout]")
{
  CHECK(static_cast<int>(LayoutKind::Custom) == 0);
  CHECK(static_cast<int>(LayoutKind::FourUp) == 1);
  CHECK(static_cast<int>(LayoutKind::ThreeUp) == 2);
  CHECK(static_cast<int>(LayoutKind::OneUp) == 3);
  CHECK(static_cast<int>(LayoutKind::MultiImageGrid) == 4);
  CHECK(static_cast<int>(LayoutKind::AxCorSagByImage) == 5);
  CHECK(static_cast<int>(LayoutKind::Lightbox) == 6);
  CHECK(static_cast<int>(LayoutKind::NumElements) == 7);
}

TEST_CASE("camera sync mode enum ordinals remain stable for layout sync specs", "[viewer][layout]")
{
  CHECK(static_cast<int>(CameraSyncMode::Rotation) == 0);
  CHECK(static_cast<int>(CameraSyncMode::Translation) == 1);
  CHECK(static_cast<int>(CameraSyncMode::Zoom) == 2);
}
