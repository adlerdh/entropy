#include "rendering/helpers/ImageDrawingHelpers.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

namespace image_drawing = rendering::image_drawing;

namespace
{

void requireVec3(const glm::vec3& value, float x, float y, float z)
{
  REQUIRE(value.x == Catch::Approx(x));
  REQUIRE(value.y == Catch::Approx(y));
  REQUIRE(value.z == Catch::Approx(z));
}

} // namespace

TEST_CASE("image drawing view-axis sampling direction uses inverse pixel dimensions", "[rendering][image-drawing]")
{
  const glm::mat4 pixel_T_clip{1.0f};
  const glm::vec3 invPixelDimensions{0.5f, 0.25f, 0.125f};

  requireVec3(
    image_drawing::computeTextureSamplingDirectionForViewAxis(
      pixel_T_clip,
      invPixelDimensions,
      Directions::View::Right),
    0.5f,
    0.0f,
    0.0f);

  requireVec3(
    image_drawing::computeTextureSamplingDirectionForViewAxis(pixel_T_clip, invPixelDimensions, Directions::View::Back),
    0.0f,
    0.0f,
    0.125f);

  glm::mat4 obliquePixel_T_clip{1.0f};
  obliquePixel_T_clip[0] = glm::vec4{1.0f, 1.0f, 0.0f, 0.0f};
  requireVec3(
    image_drawing::computeTextureSamplingDirectionForViewAxis(
      obliquePixel_T_clip,
      invPixelDimensions,
      Directions::View::Right),
    0.353553f,
    0.176777f,
    0.0f);
}

TEST_CASE("image drawing view-pixel sampling direction follows viewport scaling", "[rendering][image-drawing]")
{
  const Viewport windowViewport{0.0f, 0.0f, 200.0f, 100.0f};

  requireVec3(
    image_drawing::computeTextureSamplingDirectionForViewPixelOffset(
      glm::mat4{1.0f},
      windowViewport,
      glm::mat4{1.0f},
      glm::vec2{2.0f, 4.0f}),
    0.02f,
    0.08f,
    0.0f);
}

TEST_CASE("image drawing voxel sampling direction normalizes projected view direction", "[rendering][image-drawing]")
{
  const Viewport windowViewport{0.0f, 0.0f, 100.0f, 100.0f};
  const glm::vec3 invPixelDimensions{0.5f, 0.25f, 0.125f};

  requireVec3(
    image_drawing::computeTextureSamplingDirectionForImageVoxelOffset(
      glm::mat4{1.0f},
      windowViewport,
      glm::mat4{1.0f},
      invPixelDimensions,
      glm::vec2{10.0f, 0.0f}),
    0.5f,
    0.0f,
    0.0f);

  requireVec3(
    image_drawing::computeTextureSamplingDirectionForImageVoxelOffset(
      glm::mat4{1.0f},
      windowViewport,
      glm::mat4{1.0f},
      invPixelDimensions,
      glm::vec2{10.0f, 10.0f}),
    0.353553f,
    0.176777f,
    0.0f);

  requireVec3(
    image_drawing::computeTextureSamplingDirectionForImageVoxelOffset(
      glm::mat4{1.0f},
      windowViewport,
      glm::mat4{1.0f},
      invPixelDimensions,
      glm::vec2{0.0f}),
    0.0f,
    0.0f,
    0.0f);
}

TEST_CASE("image drawing MIP sampling parameters cover slab and max-extent modes", "[rendering][image-drawing]")
{
  const auto slabParams = image_drawing::computeMipSamplingParams(2.0f, glm::uvec3{3, 4, 12}, 10.0f, false);
  REQUIRE(slabParams.halfNumSamples == 2);
  REQUIRE(slabParams.samplingDistanceCm == Catch::Approx(0.2f));

  const auto maxExtentParams = image_drawing::computeMipSamplingParams(2.0f, glm::uvec3{3, 4, 12}, 10.0f, true);
  REQUIRE(maxExtentParams.halfNumSamples == 13);
  REQUIRE(maxExtentParams.samplingDistanceCm == Catch::Approx(0.2f));

  const auto invalidParams = image_drawing::computeMipSamplingParams(0.0f, glm::uvec3{3, 4, 12}, 10.0f, false);
  REQUIRE(invalidParams.halfNumSamples == 0);
  REQUIRE(invalidParams.samplingDistanceCm == Catch::Approx(0.0f));
}

TEST_CASE("image edge pass planning handles supported and offscreen targets", "[rendering][image-drawing][pixel-edge]")
{
  auto plan = image_drawing::computeEdgePassPlan(false, false, false, true);
  CHECK(plan.drawImageDirectly);
  CHECK_FALSE(plan.drawScreenPixelEdges);
  CHECK_FALSE(plan.drawVoxelEdges);

  plan = image_drawing::computeEdgePassPlan(false, true, false, true);
  CHECK_FALSE(plan.drawImageDirectly);
  CHECK(plan.drawScreenPixelEdges);
  CHECK_FALSE(plan.drawVoxelEdges);

  plan = image_drawing::computeEdgePassPlan(false, true, false, false);
  CHECK(plan.drawImageDirectly);
  CHECK_FALSE(plan.drawScreenPixelEdges);
  CHECK_FALSE(plan.disableIntensityProjectionForDirectImage);

  plan = image_drawing::computeEdgePassPlan(true, false, false, true);
  CHECK_FALSE(plan.drawImageDirectly);
  CHECK(plan.drawVoxelEdges);

  plan = image_drawing::computeEdgePassPlan(true, false, true, true);
  CHECK(plan.drawImageDirectly);
  CHECK(plan.drawVoxelEdges);
  CHECK(plan.disableIntensityProjectionForDirectImage);
}

TEST_CASE("image drawing estimates screen pixels per voxel axis", "[rendering][image-drawing]")
{
  const Viewport windowViewport{0.0f, 0.0f, 200.0f, 100.0f};
  REQUIRE(
    image_drawing::maxScreenPixelsPerVoxelAxis(glm::mat4{1.0f}, glm::mat4{1.0f}, windowViewport) ==
    Catch::Approx(100.0f));

  const glm::mat4
    viewClip_T_voxel{0.25f, 0.0f, 0.0f, 0.0f, 0.0f, 0.5f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f};
  REQUIRE(
    image_drawing::maxScreenPixelsPerVoxelAxis(viewClip_T_voxel, glm::mat4{1.0f}, windowViewport) ==
    Catch::Approx(25.0f));
}

TEST_CASE(
  "image drawing automatic floating-point interpolation uses magnification threshold",
  "[rendering][image-drawing]")
{
  REQUIRE_FALSE(image_drawing::automaticFloatingPointInterpolationEnabled(127.9f, 128.0f));
  REQUIRE(image_drawing::automaticFloatingPointInterpolationEnabled(128.0f, 128.0f));
  REQUIRE_FALSE(image_drawing::automaticFloatingPointInterpolationEnabled(0.0f, 128.0f));
}
