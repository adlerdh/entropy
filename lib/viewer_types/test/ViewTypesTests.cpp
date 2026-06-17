#include "viewer_types/ViewTypes.h"

#include <catch2/catch_test_macros.hpp>

#include <array>

TEST_CASE("all view types are advertised in stable UI order", "[viewer_types][view]")
{
  constexpr std::array expected{
    ViewType::Axial,
    ViewType::Coronal,
    ViewType::Sagittal,
    ViewType::Oblique,
    ViewType::ThreeD};

  REQUIRE(AllViewTypes == expected);
}

TEST_CASE("view type enum ordinals remain stable for serialized layout specs", "[viewer_types][view]")
{
  CHECK(static_cast<int>(ViewType::Axial) == 0);
  CHECK(static_cast<int>(ViewType::Coronal) == 1);
  CHECK(static_cast<int>(ViewType::Sagittal) == 2);
  CHECK(static_cast<int>(ViewType::Oblique) == 3);
  CHECK(static_cast<int>(ViewType::ThreeD) == 4);
  CHECK(static_cast<int>(ViewType::NumElements) == 5);
}

TEST_CASE("view type display strings reflect crosshairs rotation state", "[viewer_types][view]")
{
  CHECK(to_string(ViewType::Axial, false) == "Axial");
  CHECK(to_string(ViewType::Coronal, false) == "Coronal");
  CHECK(to_string(ViewType::Sagittal, false) == "Sagittal");
  CHECK(to_string(ViewType::Oblique, false) == "Oblique");
  CHECK(to_string(ViewType::ThreeD, false) == "3D");

  CHECK(to_string(ViewType::Axial, true) == "Z");
  CHECK(to_string(ViewType::Coronal, true) == "Y");
  CHECK(to_string(ViewType::Sagittal, true) == "X");
  CHECK(to_string(ViewType::Oblique, true) == "Oblique");
  CHECK(to_string(ViewType::ThreeD, true) == "3D");
}

TEST_CASE("view type display strings tolerate sentinel values", "[viewer_types][view]")
{
  CHECK(to_string(ViewType::NumElements, false) == "Unknown");
  CHECK(to_string(static_cast<ViewType>(100), true) == "Unknown");
}
