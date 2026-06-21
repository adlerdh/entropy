#include "rendering/LightboxOffsetLabelFormat.h"

#include <catch2/catch_test_macros.hpp>

namespace lightbox = entropy::rendering::lightbox;

TEST_CASE("lightbox offset labels use the reference offset unit for every tile", "[rendering][lightbox]")
{
  const double referenceOffsetMm = 0.1;

  CHECK(lightbox::formatOffsetDistanceMm(0.1, referenceOffsetMm) == "+0.1 mm");
  CHECK(lightbox::formatOffsetDistanceMm(1.0, referenceOffsetMm) == "+1 mm");
  CHECK(lightbox::formatOffsetDistanceMm(25.0, referenceOffsetMm) == "+25 mm");
  CHECK(lightbox::formatOffsetDistanceMm(0.0, referenceOffsetMm) == "0 mm");
  CHECK(lightbox::formatOffsetDistanceMm(-0.25, referenceOffsetMm) == "-0.25 mm");
}

TEST_CASE("lightbox offset labels prefer millimeters for common slice spacing", "[rendering][lightbox]")
{
  const double sliceSpacingMm = 2.0;

  CHECK(lightbox::formatOffsetDistanceMm(2.0, sliceSpacingMm) == "+2 mm");
  CHECK(lightbox::formatOffsetDistanceMm(4.0, sliceSpacingMm) == "+4 mm");
  CHECK(lightbox::formatOffsetDistanceMm(20.0, sliceSpacingMm) == "+20 mm");
}

TEST_CASE(
  "lightbox offset labels keep centimeter units when the reference offset is centimeters",
  "[rendering][lightbox]")
{
  const double referenceOffsetMm = -25.0;

  CHECK(lightbox::formatOffsetDistanceMm(-25.0, referenceOffsetMm) == "-2.5 cm");
  CHECK(lightbox::formatOffsetDistanceMm(0.0, referenceOffsetMm) == "0 cm");
  CHECK(lightbox::formatOffsetDistanceMm(5.0, referenceOffsetMm) == "+0.5 cm");
}

TEST_CASE("lightbox offset labels switch to centimeters for large slice spacing", "[rendering][lightbox]")
{
  const double sliceSpacingMm = 10.0;

  CHECK(lightbox::formatOffsetDistanceMm(10.0, sliceSpacingMm) == "+1 cm");
  CHECK(lightbox::formatOffsetDistanceMm(20.0, sliceSpacingMm) == "+2 cm");
  CHECK(lightbox::formatOffsetDistanceMm(2.0, sliceSpacingMm) == "+0.2 cm");
}

TEST_CASE("lightbox offset labels default to millimeters when the reference offset is zero", "[rendering][lightbox]")
{
  const double referenceOffsetMm = 0.0;

  CHECK(lightbox::formatOffsetDistanceMm(0.0, referenceOffsetMm) == "0 mm");
  CHECK(lightbox::formatOffsetDistanceMm(25.0, referenceOffsetMm) == "+25 mm");
}

TEST_CASE("lightbox offset labels keep microscopic units from the first offset", "[rendering][lightbox]")
{
  CHECK(lightbox::formatOffsetDistanceMm(0.0005, 0.0005) == "+0.5 µm");
  CHECK(lightbox::formatOffsetDistanceMm(0.001, 0.0005) == "+1 µm");
  CHECK(lightbox::formatOffsetDistanceMm(0.0, 0.0000005) == "0 nm");
  CHECK(lightbox::formatOffsetDistanceMm(0.000001, 0.0000005) == "+1 nm");
}
