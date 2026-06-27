#include "image/Image.h"
#include "image/WarpInversion.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <glm/glm.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace
{
uint32_t componentSizeInBytes(ComponentType componentType)
{
  switch (componentType) {
    case ComponentType::Float32:
      return 4;
    default:
      return 0;
  }
}

ImageIoInfo makeVectorFieldIoInfo(glm::uvec3 dims)
{
  ImageIoInfo info;
  info.m_fileInfo.m_fileName = "warp.nrrd";
  info.m_fileInfo.m_fileTypeString = "Nrrd";

  info.m_componentInfo.m_componentType = ComponentType::Float32;
  info.m_componentInfo.m_componentTypeString = componentTypeString(ComponentType::Float32);
  info.m_componentInfo.m_componentSizeInBytes = componentSizeInBytes(ComponentType::Float32);

  info.m_pixelInfo.m_pixelType = PixelType::Vector;
  info.m_pixelInfo.m_pixelTypeString = "vector";
  info.m_pixelInfo.m_numComponents = 3;
  info.m_pixelInfo.m_pixelStrideInBytes = 3 * sizeof(float);

  info.m_sizeInfo.m_imageSizeInPixels = static_cast<std::size_t>(dims.x) * dims.y * dims.z;
  info.m_sizeInfo.m_imageSizeInComponents = 3 * info.m_sizeInfo.m_imageSizeInPixels;
  info.m_sizeInfo.m_imageSizeInBytes = info.m_sizeInfo.m_imageSizeInComponents * sizeof(float);

  info.m_spaceInfo.m_numDimensions = 3;
  info.m_spaceInfo.m_dimensions = {dims.x, dims.y, dims.z};
  info.m_spaceInfo.m_origin = {0.0, 0.0, 0.0};
  info.m_spaceInfo.m_spacing = {1.0, 1.0, 1.0};
  info.m_spaceInfo.m_directions = {{1.0, 0.0, 0.0}, {0.0, 1.0, 0.0}, {0.0, 0.0, 1.0}};
  return info;
}

Image makeConstantWarp(glm::uvec3 dims, glm::vec3 displacement, const std::string& name)
{
  ImageIoInfo ioInfo = makeVectorFieldIoInfo(dims);
  ImageHeader header(ioInfo, ioInfo, false);
  const std::size_t numPixels = ioInfo.m_sizeInfo.m_imageSizeInPixels;
  std::vector<float> x(numPixels, displacement.x);
  std::vector<float> y(numPixels, displacement.y);
  std::vector<float> z(numPixels, displacement.z);
  std::vector<const void*> buffers{x.data(), y.data(), z.data()};
  return Image(
    header,
    name,
    Image::ImageRepresentation::Image,
    Image::MultiComponentBufferType::SeparateImages,
    buffers);
}

WarpInversionOptions testOptions()
{
  WarpInversionOptions options;
  options.maxIterations = 80;
  options.meanErrorTolerance = 1.0e-5;
  options.maxErrorTolerance = 1.0e-4;
  options.enforceBoundaryCondition = false;
  return options;
}
} // namespace

TEST_CASE("Computed warp display names identify direction and source", "[image][warp]")
{
  const Image source = makeConstantWarp({3, 3, 3}, {1.0f, 0.0f, 0.0f}, "moving-to-reference");

  CHECK(
    computedWarpDisplayName(source, ComputedWarpDirection::Inverse) == "Computed inverse warp - moving-to-reference");
  CHECK(
    computedWarpDisplayName(source, ComputedWarpDirection::Forward) == "Computed forward warp - moving-to-reference");
}

TEST_CASE("Zero warp inversion produces a zero matching field", "[image][warp]")
{
  const Image source = makeConstantWarp({3, 3, 3}, {0.0f, 0.0f, 0.0f}, "zero-warp");

  const auto result = computeMatchingWarp(source, source, ComputedWarpDirection::Inverse, testOptions());
  REQUIRE(result.has_value());
  CHECK(result->direction == ComputedWarpDirection::Inverse);
  CHECK(result->image.header().numComponentsPerPixel() == 3);
  CHECK(result->image.value<float>(0, 13).value() == Catch::Approx(0.0f));
  CHECK(result->image.value<float>(1, 13).value() == Catch::Approx(0.0f));
  CHECK(result->image.value<float>(2, 13).value() == Catch::Approx(0.0f));
  CHECK(result->report.meanResidualMm == Catch::Approx(0.0));
  CHECK(result->report.maxResidualMm == Catch::Approx(0.0));
}

TEST_CASE("Constant translation warp inversion changes displacement sign", "[image][warp]")
{
  const glm::vec3 translation{0.25f, -0.5f, 0.75f};
  const Image source = makeConstantWarp({5, 5, 5}, translation, "constant-forward-warp");

  const auto result = computeMatchingWarp(source, source, ComputedWarpDirection::Inverse, testOptions());
  REQUIRE(result.has_value());

  const std::size_t center = 2 + 5 * (2 + 5 * 2);
  CHECK(result->image.value<float>(0, center).value() == Catch::Approx(-translation.x).margin(1.0e-3f));
  CHECK(result->image.value<float>(1, center).value() == Catch::Approx(-translation.y).margin(1.0e-3f));
  CHECK(result->image.value<float>(2, center).value() == Catch::Approx(-translation.z).margin(1.0e-3f));
  CHECK(result->report.meanResidualMm < 1.0e-1);
}
