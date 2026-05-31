#include "MathFuncs.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <glm/glm.hpp>

TEST_CASE("subject image dimensions are pixel dimensions scaled by spacing", "[common][math]")
{
  const glm::dvec3 dims = math::computeSubjectImageDimensions({4, 5, 6}, {0.5, 2.0, 3.0});

  CHECK(dims.x == Catch::Approx(2.0));
  CHECK(dims.y == Catch::Approx(10.0));
  CHECK(dims.z == Catch::Approx(18.0));
}

TEST_CASE("pixel-to-texture transform maps voxel centers into normalized texture coordinates", "[common][math]")
{
  const glm::dmat4 pixel_T_texture = math::computeImagePixelToTextureTransformation({4, 5, 2});

  const glm::dvec4 first = pixel_T_texture * glm::dvec4{0.0, 0.0, 0.0, 1.0};
  const glm::dvec4 last = pixel_T_texture * glm::dvec4{3.0, 4.0, 1.0, 1.0};

  CHECK(first.x == Catch::Approx(0.125));
  CHECK(first.y == Catch::Approx(0.1));
  CHECK(first.z == Catch::Approx(0.25));

  CHECK(last.x == Catch::Approx(0.875));
  CHECK(last.y == Catch::Approx(0.9));
  CHECK(last.z == Catch::Approx(0.75));
}
