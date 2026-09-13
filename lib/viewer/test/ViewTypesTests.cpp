#include "viewer/ViewTypes.h"

#include "common/Types.h"

#include <catch2/catch_test_macros.hpp>

#include <array>

TEST_CASE("all view types are advertised in stable UI order", "[viewer][view]")
{
  constexpr std::array expected{
    ViewType::Axial,
    ViewType::Coronal,
    ViewType::Sagittal,
    ViewType::Oblique,
    ViewType::ThreeD};

  REQUIRE(AllViewTypes == expected);
}

TEST_CASE("view type enum ordinals remain stable for serialized layout specs", "[viewer][view]")
{
  CHECK(static_cast<int>(ViewType::Axial) == 0);
  CHECK(static_cast<int>(ViewType::Coronal) == 1);
  CHECK(static_cast<int>(ViewType::Sagittal) == 2);
  CHECK(static_cast<int>(ViewType::Oblique) == 3);
  CHECK(static_cast<int>(ViewType::ThreeD) == 4);
  CHECK(static_cast<int>(ViewType::NumElements) == 5);
}

TEST_CASE("view type display strings reflect anatomical direction convention", "[viewer][view]")
{
  CHECK(viewTypeDisplayName(ViewType::Axial, AnatomicalLabelType::Human, false) == "Axial");
  CHECK(viewTypeDisplayName(ViewType::Coronal, AnatomicalLabelType::Human, false) == "Coronal");
  CHECK(viewTypeDisplayName(ViewType::Sagittal, AnatomicalLabelType::Human, false) == "Sagittal");

  CHECK(viewTypeDisplayName(ViewType::Axial, AnatomicalLabelType::Rodent, false) == "Coronal");
  CHECK(viewTypeDisplayName(ViewType::Coronal, AnatomicalLabelType::Rodent, false) == "Horizontal");
  CHECK(viewTypeDisplayName(ViewType::Sagittal, AnatomicalLabelType::Rodent, false) == "Sagittal");

  CHECK(viewTypeDisplayName(ViewType::Axial, AnatomicalLabelType::Cartesian, false) == "Z");
  CHECK(viewTypeDisplayName(ViewType::Coronal, AnatomicalLabelType::Cartesian, false) == "Y");
  CHECK(viewTypeDisplayName(ViewType::Sagittal, AnatomicalLabelType::Cartesian, false) == "X");

  CHECK(viewTypeDisplayName(ViewType::Axial, AnatomicalLabelType::Quadruped, false) == "Transverse");
  CHECK(viewTypeDisplayName(ViewType::Coronal, AnatomicalLabelType::Quadruped, false) == "Dorsal");
  CHECK(viewTypeDisplayName(ViewType::Sagittal, AnatomicalLabelType::Quadruped, false) == "Sagittal");

  CHECK(viewTypeDisplayName(ViewType::Oblique, AnatomicalLabelType::Rodent, false) == "Oblique");
  CHECK(viewTypeDisplayName(ViewType::ThreeD, AnatomicalLabelType::Cartesian, false) == "3D");
}

TEST_CASE("rotated crosshairs use primed local-axis view names", "[viewer][view]")
{
  for (const AnatomicalLabelType convention :
       {AnatomicalLabelType::Human, AnatomicalLabelType::Rodent, AnatomicalLabelType::Cartesian})
  {
    CHECK(viewTypeDisplayName(ViewType::Axial, convention, true) == "Z\xE2\x80\xB2");
    CHECK(viewTypeDisplayName(ViewType::Coronal, convention, true) == "Y\xE2\x80\xB2");
    CHECK(viewTypeDisplayName(ViewType::Sagittal, convention, true) == "X\xE2\x80\xB2");
    CHECK(viewTypeDisplayName(ViewType::Oblique, convention, true) == "Oblique");
    CHECK(viewTypeDisplayName(ViewType::ThreeD, convention, true) == "3D");
  }
}

TEST_CASE("view type display strings tolerate sentinel values", "[viewer][view]")
{
  CHECK(viewTypeDisplayName(ViewType::NumElements, AnatomicalLabelType::Human, false) == "Unknown");
  // Deliberately exercise the defensive default branch with a value outside the enum's declared range.
  const auto invalidViewType = static_cast<ViewType>(100); // NOLINT(clang-analyzer-optin.core.EnumCastOutOfRange)
  CHECK(viewTypeDisplayName(invalidViewType, AnatomicalLabelType::Cartesian, true) == "Unknown");
}
