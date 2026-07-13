#include "ui/IsosurfaceRangeModel.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/color_space.hpp>

using Catch::Approx;

TEST_CASE("Isosurface range spacing is implied by inclusive endpoints and count")
{
  CHECK(ui::isosurfaceRangeSpacing(10.0, 20.0, 1) == Approx(0.0));
  CHECK(ui::isosurfaceRangeSpacing(10.0, 20.0, 6) == Approx(2.0));
  CHECK(ui::isosurfaceRangeSpacing(20.0, 10.0, 6) == Approx(2.0));
}

TEST_CASE("Isosurface range count chooses nearest spacing-compatible count")
{
  CHECK(ui::isosurfaceRangeCountForSpacing(0.0, 10.0, 2.0) == 6);
  CHECK(ui::isosurfaceRangeCountForSpacing(0.0, 10.0, 3.0) == 4);
  CHECK(ui::isosurfaceRangeCountForSpacing(5.0, 5.0, 1.0) == 1);
  CHECK(ui::isosurfaceRangeCountForSpacing(0.0, 10.0, 0.0) == 2);
}

TEST_CASE("Isosurface range values include start and end")
{
  ui::IsosurfaceRangeParameters params;
  params.start = 2.0;
  params.end = 8.0;
  params.count = 4;

  const auto values = ui::isosurfaceRangeValues(params);

  REQUIRE(values.size() == 4);
  CHECK(values[0] == Approx(2.0));
  CHECK(values[1] == Approx(4.0));
  CHECK(values[2] == Approx(6.0));
  CHECK(values[3] == Approx(8.0));
}

TEST_CASE("Isosurface range values support descending ranges")
{
  ui::IsosurfaceRangeParameters params;
  params.start = 8.0;
  params.end = 2.0;
  params.count = 4;

  const auto values = ui::isosurfaceRangeValues(params);

  REQUIRE(values.size() == 4);
  CHECK(values[0] == Approx(8.0));
  CHECK(values[1] == Approx(6.0));
  CHECK(values[2] == Approx(4.0));
  CHECK(values[3] == Approx(2.0));
}

TEST_CASE("Default isosurface names pad one-digit indices")
{
  CHECK(ui::defaultIsosurfaceName(1) == "Surface 01");
  CHECK(ui::defaultIsosurfaceName(9) == "Surface 09");
  CHECK(ui::defaultIsosurfaceName(10) == "Surface 10");
  CHECK(ui::defaultIsosurfaceName(100) == "Surface 100");
}

TEST_CASE("HSV color interpolation keeps endpoints and interpolates hue")
{
  const glm::vec3 red{1.0f, 0.0f, 0.0f};
  const glm::vec3 green{0.0f, 1.0f, 0.0f};

  const glm::vec3 start = ui::interpolateHsvColor(red, green, 0.0);
  const glm::vec3 middle = ui::interpolateHsvColor(red, green, 0.5);
  const glm::vec3 end = ui::interpolateHsvColor(red, green, 1.0);

  CHECK(start.r == Approx(1.0f));
  CHECK(start.g == Approx(0.0f).margin(1.0e-6f));
  CHECK(start.b == Approx(0.0f).margin(1.0e-6f));

  CHECK(middle.r == Approx(1.0f));
  CHECK(middle.g == Approx(1.0f));
  CHECK(middle.b == Approx(0.0f).margin(1.0e-6f));

  CHECK(end.r == Approx(0.0f).margin(1.0e-6f));
  CHECK(end.g == Approx(1.0f));
  CHECK(end.b == Approx(0.0f).margin(1.0e-6f));
}

TEST_CASE("HSV color interpolation follows the shortest hue path")
{
  const glm::vec3 magenta = glm::rgbColor(glm::vec3{300.0f, 1.0f, 1.0f});
  const glm::vec3 red = glm::rgbColor(glm::vec3{0.0f, 1.0f, 1.0f});

  const glm::vec3 middle = ui::interpolateHsvColor(magenta, red, 0.5);
  const glm::vec3 middleHsv = glm::hsvColor(middle);

  CHECK(middleHsv.x == Approx(330.0f));
  CHECK(middleHsv.y == Approx(1.0f));
  CHECK(middleHsv.z == Approx(1.0f));
}
