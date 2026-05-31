#include "ImageColorMap.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <sstream>
#include <vector>

TEST_CASE("RGB color maps are promoted to opaque RGBA", "[image][colormap]")
{
  ImageColorMap map(
    "test",
    "technical",
    "description",
    ImageColorMap::InterpolationMode::Linear,
    std::vector<glm::vec3>{{0.25f, 0.5f, 0.75f}});

  REQUIRE(map.numColors() == 1);

  const glm::vec4 color = map.color_RGBA_F32(0);
  CHECK(color.r == Catch::Approx(0.25f));
  CHECK(color.g == Catch::Approx(0.5f));
  CHECK(color.b == Catch::Approx(0.75f));
  CHECK(color.a == Catch::Approx(1.0f));
}

TEST_CASE("CSV color maps preserve explicit alpha", "[image][colormap]")
{
  std::istringstream csv{
    "Brief Name\n"
    "Technical Name\n"
    "Description\n"
    "0.1,0.2,0.3,0.4\n"
    "0.5,0.6,0.7\n"};

  auto map = ImageColorMap::loadImageColorMap(csv);
  REQUIRE(map.has_value());
  REQUIRE(map->numColors() == 2);

  CHECK(map->color_RGBA_F32(0).a == Catch::Approx(0.4f));
  CHECK(map->color_RGBA_F32(1).a == Catch::Approx(1.0f));
}
