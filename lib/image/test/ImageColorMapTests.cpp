#include "image/ImageColorMap.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <cstddef>
#include <sstream>
#include <vector>

namespace
{

void checkColor(const glm::vec4& actual, const glm::vec4& expected)
{
  CHECK(actual.r == Catch::Approx(expected.r));
  CHECK(actual.g == Catch::Approx(expected.g));
  CHECK(actual.b == Catch::Approx(expected.b));
  CHECK(actual.a == Catch::Approx(expected.a));
}

ImageColorMap makeFourColorMap()
{
  return ImageColorMap(
    "test",
    "technical",
    "description",
    ImageColorMap::InterpolationMode::Nearest,
    std::vector<glm::vec4>{
      {0.0f, 0.0f, 0.0f, 1.0f},
      {1.0f, 0.0f, 0.0f, 1.0f},
      {0.0f, 1.0f, 0.0f, 1.0f},
      {0.0f, 0.0f, 1.0f, 1.0f}});
}

} // namespace

TEST_CASE("Color maps require at least one color", "[image][colormap]")
{
  CHECK_THROWS(
    ImageColorMap("empty", "empty", "empty", ImageColorMap::InterpolationMode::Linear, std::vector<glm::vec3>{}));

  CHECK_THROWS(
    ImageColorMap("empty", "empty", "empty", ImageColorMap::InterpolationMode::Linear, std::vector<glm::vec4>{}));
}

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

TEST_CASE("RGBA color maps expose metadata and contiguous float storage", "[image][colormap]")
{
  const std::vector<glm::vec4> colors{{0.1f, 0.2f, 0.3f, 0.4f}, {0.5f, 0.6f, 0.7f, 0.8f}};

  ImageColorMap map(
    "Display",
    "display_technical",
    "Display description",
    ImageColorMap::InterpolationMode::Nearest,
    colors);

  CHECK(map.name() == "Display");
  CHECK(map.technicalName() == "display_technical");
  CHECK(map.description() == "Display description");
  CHECK(map.interpolationMode() == ImageColorMap::InterpolationMode::Nearest);
  REQUIRE(map.numColors() == colors.size());
  CHECK(map.numBytes_RGBA_F32() == colors.size() * sizeof(glm::vec4));
  CHECK(map.data_RGBA_asVector().size() == colors.size());

  const float* data = map.data_RGBA_F32();
  REQUIRE(data != nullptr);
  CHECK(data[0] == Catch::Approx(0.1f));
  CHECK(data[1] == Catch::Approx(0.2f));
  CHECK(data[2] == Catch::Approx(0.3f));
  CHECK(data[3] == Catch::Approx(0.4f));
  CHECK(data[4] == Catch::Approx(0.5f));
  CHECK(data[7] == Catch::Approx(0.8f));
}

TEST_CASE("Preview maps are optional contiguous RGBA buffers", "[image][colormap]")
{
  ImageColorMap map = makeFourColorMap();

  CHECK_FALSE(map.hasPreviewMap());
  CHECK(map.numPreviewMapColors() == 0);

  map.setPreviewMap({{0.2f, 0.3f, 0.4f, 0.5f}, {0.6f, 0.7f, 0.8f, 0.9f}});

  REQUIRE(map.hasPreviewMap());
  REQUIRE(map.numPreviewMapColors() == 2);

  const float* preview = map.getPreviewMap();
  REQUIRE(preview != nullptr);
  CHECK(preview[0] == Catch::Approx(0.2f));
  CHECK(preview[3] == Catch::Approx(0.5f));
  CHECK(preview[4] == Catch::Approx(0.6f));
  CHECK(preview[7] == Catch::Approx(0.9f));
}

TEST_CASE("Color maps validate and mutate indexed colors", "[image][colormap]")
{
  ImageColorMap map = makeFourColorMap();

  CHECK_THROWS(map.color_RGBA_F32(map.numColors()));

  CHECK(map.setColorRGBA(1, {0.25f, 0.5f, 0.75f, 0.125f}));
  checkColor(map.color_RGBA_F32(1), {0.25f, 0.5f, 0.75f, 0.125f});

  CHECK_FALSE(map.setColorRGBA(map.numColors(), {1.0f, 1.0f, 1.0f, 1.0f}));
}

TEST_CASE("Color map sampling flags are explicit", "[image][colormap]")
{
  ImageColorMap map = makeFourColorMap();

  checkColor(map.color_RGBA_F32(0), {0.0f, 0.0f, 0.0f, 1.0f});

  CHECK(map.slopeIntercept(false).x == Catch::Approx(1.0f));
  CHECK(map.slopeIntercept(false).y == Catch::Approx(0.0f));
  CHECK(map.slopeIntercept(true).x == Catch::Approx(-1.0f));
  CHECK(map.slopeIntercept(true).y == Catch::Approx(1.0f));

  CHECK_FALSE(map.transparentBorder());
  map.setTransparentBorder(true);
  CHECK(map.transparentBorder());
  map.setTransparentBorder(false);
  CHECK_FALSE(map.transparentBorder());

  map.setInterpolationMode(ImageColorMap::InterpolationMode::Linear);
  CHECK(map.interpolationMode() == ImageColorMap::InterpolationMode::Linear);
}

TEST_CASE("Color maps can be reversed and cyclically rotated", "[image][colormap]")
{
  ImageColorMap map = makeFourColorMap();

  map.reverse();
  checkColor(map.color_RGBA_F32(0), {0.0f, 0.0f, 1.0f, 1.0f});
  checkColor(map.color_RGBA_F32(3), {0.0f, 0.0f, 0.0f, 1.0f});

  map = makeFourColorMap();
  map.cyclicRotate(0.5f);
  checkColor(map.color_RGBA_F32(0), {0.0f, 1.0f, 0.0f, 1.0f});
  checkColor(map.color_RGBA_F32(3), {1.0f, 0.0f, 0.0f, 1.0f});

  map = makeFourColorMap();
  map.cyclicRotate(1.25f);
  checkColor(map.color_RGBA_F32(0), {1.0f, 0.0f, 0.0f, 1.0f});

  map = makeFourColorMap();
  map.cyclicRotate(-0.25f);
  checkColor(map.color_RGBA_F32(0), {0.0f, 0.0f, 1.0f, 1.0f});
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

TEST_CASE("CSV color maps sanitize metadata and parse quoted numeric fields", "[image][colormap]")
{
  std::istringstream csv{
    "Brief! Name?\n"
    "Technical@ Name#\n"
    "Description: with $ punctuation.\n"
    "\"0.1\",\"0.2\",\"0.3\",\"0.4\"\n"};

  auto map = ImageColorMap::loadImageColorMap(csv);
  REQUIRE(map.has_value());

  CHECK(map->name() == "Brief Name");
  CHECK(map->technicalName() == "Technical Name");
  CHECK(map->description() == "Description with  punctuation");
  checkColor(map->color_RGBA_F32(0), {0.1f, 0.2f, 0.3f, 0.4f});
}

TEST_CASE("CSV color map loading rejects invalid input", "[image][colormap]")
{
  {
    std::istringstream csv{"Only name\n"};
    CHECK_FALSE(ImageColorMap::loadImageColorMap(csv).has_value());
  }

  {
    std::istringstream csv{
      "Name\n"
      "Technical\n"
      "Description\n"};
    CHECK_FALSE(ImageColorMap::loadImageColorMap(csv).has_value());
  }

  {
    std::istringstream csv{
      "Name\n"
      "Technical\n"
      "Description\n"
      "0.1,0.2\n"};
    CHECK_FALSE(ImageColorMap::loadImageColorMap(csv).has_value());
  }

  {
    std::istringstream csv{
      "Name\n"
      "Technical\n"
      "Description\n"
      "0.1,not-a-number,0.3\n"};
    CHECK_FALSE(ImageColorMap::loadImageColorMap(csv).has_value());
  }
}

TEST_CASE("Linear color map factory interpolates endpoints and creates a preview", "[image][colormap]")
{
  ImageColorMap map = ImageColorMap::createLinearImageColorMap(
    {0.0f, 0.25f, 0.5f, 0.75f},
    {1.0f, 0.75f, 0.5f, 0.25f},
    3,
    "Linear",
    "Linear description",
    "linear_technical");

  CHECK(map.name() == "Linear");
  CHECK(map.technicalName() == "linear_technical");
  CHECK(map.description() == "Linear description");
  CHECK(map.interpolationMode() == ImageColorMap::InterpolationMode::Linear);

  REQUIRE(map.numColors() == 3);
  checkColor(map.color_RGBA_F32(0), {0.0f, 0.25f, 0.5f, 0.75f});
  checkColor(map.color_RGBA_F32(1), {0.5f, 0.5f, 0.5f, 0.5f});
  checkColor(map.color_RGBA_F32(2), {1.0f, 0.75f, 0.5f, 0.25f});

  REQUIRE(map.hasPreviewMap());
  REQUIRE(map.numPreviewMapColors() == 64);
  const float* preview = map.getPreviewMap();
  CHECK(preview[0] == Catch::Approx(0.0f));
  CHECK(preview[4] == Catch::Approx(1.0f / 64.0f));
}

TEST_CASE("Linear color map factory promotes short maps to two colors", "[image][colormap]")
{
  ImageColorMap map = ImageColorMap::createLinearImageColorMap(
    {0.0f, 0.0f, 0.0f, 1.0f},
    {1.0f, 1.0f, 1.0f, 1.0f},
    0,
    "Linear",
    "Linear description",
    "linear_technical");

  REQUIRE(map.numColors() == 2);
  checkColor(map.color_RGBA_F32(0), {0.0f, 0.0f, 0.0f, 1.0f});
  checkColor(map.color_RGBA_F32(1), {1.0f, 1.0f, 1.0f, 1.0f});
}
