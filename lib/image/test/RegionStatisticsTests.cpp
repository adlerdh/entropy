#include "image/RegionStatistics.h"

#include "image/Image.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <iterator>
#include <limits>
#include <vector>

namespace
{
ImageHeader makeHeader(
  glm::uvec3 dimensions,
  ComponentType type,
  std::uint32_t components,
  PixelType pixelType,
  glm::vec3 spacing = {0.5f, 2.0f, 3.0f})
{
  ImageIoInfo info;
  info.m_fileInfo.m_fileName = "test.nrrd";
  info.m_fileInfo.m_fileTypeString = "Nrrd";
  info.m_pixelInfo.m_pixelType = pixelType;
  info.m_pixelInfo.m_pixelTypeString = "test";
  info.m_pixelInfo.m_numComponents = components;
  const std::uint32_t componentBytes = ComponentType::UInt8 == type ? 1u : 4u;
  info.m_pixelInfo.m_pixelStrideInBytes = components * componentBytes;
  info.m_componentInfo.m_componentType = type;
  info.m_componentInfo.m_componentTypeString = ComponentType::UInt8 == type ? "uint8" : "float";
  info.m_componentInfo.m_componentSizeInBytes = componentBytes;
  info.m_sizeInfo.m_imageSizeInPixels = static_cast<std::size_t>(dimensions.x) * dimensions.y * dimensions.z;
  info.m_sizeInfo.m_imageSizeInComponents = info.m_sizeInfo.m_imageSizeInPixels * components;
  info.m_sizeInfo.m_imageSizeInBytes = info.m_sizeInfo.m_imageSizeInComponents * componentBytes;
  info.m_spaceInfo.m_numDimensions = dimensions.z > 1u ? 3u : 2u;
  info.m_spaceInfo.m_dimensions = dimensions.z > 1u ? std::vector<std::size_t>{dimensions.x, dimensions.y, dimensions.z}
                                                    : std::vector<std::size_t>{dimensions.x, dimensions.y};
  info.m_spaceInfo.m_origin = dimensions.z > 1u ? std::vector<double>{0.0, 0.0, 0.0} : std::vector<double>{0.0, 0.0};
  info.m_spaceInfo.m_spacing = dimensions.z > 1u ? std::vector<double>{spacing.x, spacing.y, spacing.z}
                                                 : std::vector<double>{spacing.x, spacing.y};
  info.m_spaceInfo.m_directions = dimensions.z > 1u ? std::vector<std::vector<double>>{{1, 0, 0}, {0, 1, 0}, {0, 0, 1}}
                                                    : std::vector<std::vector<double>>{{1, 0}, {0, 1}};
  return ImageHeader(info, info, false);
}

template<typename T>
Image makeImage(ImageHeader header, const std::vector<std::vector<T>>& values, ImageRepresentation representation)
{
  std::vector<const void*> buffers;
  buffers.reserve(values.size());
  std::ranges::transform(values, std::back_inserter(buffers), [](const auto& value) { return value.data(); });
  return Image::fromCopiedData(
    std::move(header),
    "test",
    representation,
    MultiComponentBufferType::SeparateImages,
    buffers);
}

template<typename T>
Image makeInterleavedImage(ImageHeader header, const std::vector<T>& values)
{
  return Image::fromCopiedData(
    std::move(header),
    "test",
    ImageRepresentation::Image,
    MultiComponentBufferType::InterleavedImage,
    {values.data()});
}
} // namespace

TEST_CASE("Region statistics use population deviation and physical voxel volume", "[image][regions][statistics]")
{
  const std::vector<float> values{1.0f, 3.0f, 5.0f, 7.0f};
  const std::vector<std::uint8_t> labels{1, 1, 2, 2};
  const Image image = makeImage(
    makeHeader({2, 2, 1}, ComponentType::Float32, 1, PixelType::Scalar),
    std::vector<std::vector<float>>{values},
    ImageRepresentation::Image);
  const Image segmentation = makeImage(
    makeHeader({2, 2, 1}, ComponentType::UInt8, 1, PixelType::Scalar),
    std::vector<std::vector<std::uint8_t>>{labels},
    ImageRepresentation::Segmentation);

  const auto result = computeRegionStatistics(image, segmentation, {}, 0);
  REQUIRE(result);
  CHECK_FALSE(result->isVolume);
  CHECK_THAT(result->elementPhysicalSize, Catch::Matchers::WithinAbs(1.0, 1e-12));
  REQUIRE(result->regions.size() == 2);
  CHECK(result->regions[0].label == 1);
  CHECK(result->regions[0].elementCount == 2);
  CHECK_THAT(result->regions[0].mean, Catch::Matchers::WithinAbs(2.0, 1e-12));
  CHECK_THAT(result->regions[0].standardDeviation, Catch::Matchers::WithinAbs(1.0, 1e-12));
}

TEST_CASE("Region statistics scalarize vector and complex pixels", "[image][regions][multicomponent]")
{
  const std::vector<float> x{3.0f, 0.0f};
  const std::vector<float> y{4.0f, 1.0f};
  const std::vector<std::uint8_t> labels{1, 1};
  const Image image = makeImage(
    makeHeader({2, 1, 1}, ComponentType::Float32, 2, PixelType::Complex),
    std::vector<std::vector<float>>{x, y},
    ImageRepresentation::Image);
  const Image segmentation = makeImage(
    makeHeader({2, 1, 1}, ComponentType::UInt8, 1, PixelType::Scalar),
    std::vector<std::vector<std::uint8_t>>{labels},
    ImageRepresentation::Segmentation);

  const auto magnitude = computeRegionStatistics(image, segmentation, {RegionValueKind::ComplexMagnitude, 0}, 0);
  REQUIRE(magnitude);
  REQUIRE(magnitude->regions.size() == 1);
  CHECK_THAT(magnitude->regions[0].mean, Catch::Matchers::WithinAbs(3.0, 1e-12));

  const auto phase = computeRegionStatistics(image, segmentation, {RegionValueKind::ComplexPhase, 0}, 0);
  REQUIRE(phase);
  CHECK_THAT(phase->regions[0].minimum, Catch::Matchers::WithinAbs(std::atan2(4.0, 3.0), 1e-12));
}

TEST_CASE("Region statistics exclude non-finite intensities without losing region size", "[image][regions][finite]")
{
  const std::vector<float> values{2.0f, std::numeric_limits<float>::quiet_NaN(), 4.0f};
  const std::vector<std::uint8_t> labels{1, 1, 1};
  const Image image = makeImage(
    makeHeader({3, 1, 1}, ComponentType::Float32, 1, PixelType::Scalar),
    std::vector<std::vector<float>>{values},
    ImageRepresentation::Image);
  const Image segmentation = makeImage(
    makeHeader({3, 1, 1}, ComponentType::UInt8, 1, PixelType::Scalar),
    std::vector<std::vector<std::uint8_t>>{labels},
    ImageRepresentation::Segmentation);

  const auto result = computeRegionStatistics(image, segmentation, {}, 0);
  REQUIRE(result);
  REQUIRE(result->regions.size() == 1);
  CHECK(result->regions[0].elementCount == 3);
  CHECK(result->regions[0].finiteValueCount == 2);
  CHECK(result->regions[0].nonFiniteValueCount == 1);
  CHECK_THAT(result->regions[0].mean, Catch::Matchers::WithinAbs(3.0, 1e-12));
}

TEST_CASE("Region histograms honor label scope and include the maximum", "[image][regions][histogram]")
{
  const std::vector<float> values{0.0f, 1.0f, 2.0f, 3.0f};
  const std::vector<std::uint8_t> labels{1, 1, 2, 2};
  const Image image = makeImage(
    makeHeader({2, 2, 1}, ComponentType::Float32, 1, PixelType::Scalar),
    std::vector<std::vector<float>>{values},
    ImageRepresentation::Image);
  const Image segmentation = makeImage(
    makeHeader({2, 2, 1}, ComponentType::UInt8, 1, PixelType::Scalar),
    std::vector<std::vector<std::uint8_t>>{labels},
    ImageRepresentation::Segmentation);

  const auto histogram =
    computeRegionHistogram(image, segmentation, {}, 0, 2, {.binCount = 2, .valueRange = std::nullopt});
  REQUIRE(histogram);
  CHECK(histogram->finiteValueCount == 2);
  CHECK(histogram->counts == std::vector<double>{1.0, 1.0});
}

TEST_CASE("Region histograms honor a custom image-value range", "[image][regions][histogram]")
{
  const std::vector<float> values{0.0f, 1.0f, 2.0f, 3.0f};
  const std::vector<std::uint8_t> labels(4, 1);
  const Image image = makeImage(
    makeHeader({4, 1, 1}, ComponentType::Float32, 1, PixelType::Scalar),
    std::vector<std::vector<float>>{values},
    ImageRepresentation::Image);
  const Image segmentation = makeImage(
    makeHeader({4, 1, 1}, ComponentType::UInt8, 1, PixelType::Scalar),
    std::vector<std::vector<std::uint8_t>>{labels},
    ImageRepresentation::Segmentation);

  const auto histogram = computeRegionHistogram(
    image,
    segmentation,
    {},
    0,
    1,
    {.binCount = 2, .valueRange = std::array<double, 2>{1.0, 2.0}});
  REQUIRE(histogram);
  CHECK(histogram->finiteValueCount == 2);
  CHECK(histogram->counts == std::vector<double>{1.0, 1.0});

  const auto invalid = computeRegionHistogram(
    image,
    segmentation,
    {},
    0,
    1,
    {.binCount = 2, .valueRange = std::array<double, 2>{2.0, 1.0}});
  REQUIRE_FALSE(invalid);
  CHECK(invalid.error() == RegionStatisticsError::InvalidValueRange);
}

TEST_CASE("Region statistics support interleaved color images", "[image][regions][interleaved]")
{
  const std::vector<std::uint8_t> rgb{10, 20, 30, 20, 40, 60};
  const std::vector<std::uint8_t> labels{1, 1};
  const Image image = makeInterleavedImage(makeHeader({2, 1, 1}, ComponentType::UInt8, 3, PixelType::RGB), rgb);
  const Image segmentation = makeImage(
    makeHeader({2, 1, 1}, ComponentType::UInt8, 1, PixelType::Scalar),
    std::vector<std::vector<std::uint8_t>>{labels},
    ImageRepresentation::Segmentation);

  const auto result = computeRegionStatistics(image, segmentation, {RegionValueKind::Luminance, 0}, 0);
  REQUIRE(result);
  REQUIRE(result->regions.size() == 1);
  CHECK_THAT(result->regions[0].mean, Catch::Matchers::WithinAbs(27.894, 1e-9));
}

TEST_CASE("Region statistics reject mismatched grids", "[image][regions][validation]")
{
  const std::vector<float> values(4, 1.0f);
  const std::vector<std::uint8_t> labels(3, 1);
  const Image image = makeImage(
    makeHeader({2, 2, 1}, ComponentType::Float32, 1, PixelType::Scalar),
    std::vector<std::vector<float>>{values},
    ImageRepresentation::Image);
  const Image segmentation = makeImage(
    makeHeader({3, 1, 1}, ComponentType::UInt8, 1, PixelType::Scalar),
    std::vector<std::vector<std::uint8_t>>{labels},
    ImageRepresentation::Segmentation);

  CHECK_FALSE(computeRegionStatistics(image, segmentation, {}, 0));
  CHECK_FALSE(
    computeRegionHistogram(image, segmentation, {}, 0, std::nullopt, {.binCount = 16, .valueRange = std::nullopt}));
}

TEST_CASE("Region statistics compute exact interpolated quartiles only when requested", "[image][regions][quartiles]")
{
  const std::vector<float> values{1.0f, 2.0f, 3.0f, 4.0f};
  const std::vector<std::uint8_t> labels(4, 1);
  const Image image = makeImage(
    makeHeader({4, 1, 1}, ComponentType::Float32, 1, PixelType::Scalar),
    std::vector<std::vector<float>>{values},
    ImageRepresentation::Image);
  const Image segmentation = makeImage(
    makeHeader({4, 1, 1}, ComponentType::UInt8, 1, PixelType::Scalar),
    std::vector<std::vector<std::uint8_t>>{labels},
    ImageRepresentation::Segmentation);

  const auto basic = computeRegionStatistics(image, segmentation, {}, 0);
  REQUIRE(basic);
  CHECK_FALSE(basic->regions.front().median);

  const auto quartiles = computeRegionStatistics(image, segmentation, {}, 0, {.computeQuartiles = true});
  REQUIRE(quartiles);
  const auto& region = quartiles->regions.front();
  REQUIRE(region.firstQuartile);
  REQUIRE(region.median);
  REQUIRE(region.thirdQuartile);
  CHECK_THAT(*region.firstQuartile, Catch::Matchers::WithinAbs(1.75, 1e-12));
  CHECK_THAT(*region.median, Catch::Matchers::WithinAbs(2.5, 1e-12));
  CHECK_THAT(*region.thirdQuartile, Catch::Matchers::WithinAbs(3.25, 1e-12));
}

TEST_CASE("Region statistics distinguish spatial geometry mismatch errors", "[image][regions][validation]")
{
  const std::vector<float> values(2, 1.0f);
  const std::vector<std::uint8_t> labels(2, 1);
  ImageHeader imageHeader = makeHeader({2, 1, 1}, ComponentType::Float32, 1, PixelType::Scalar);
  ImageHeader segmentationHeader =
    makeHeader({2, 1, 1}, ComponentType::UInt8, 1, PixelType::Scalar, {0.75f, 2.0f, 3.0f});
  const Image image =
    makeImage(std::move(imageHeader), std::vector<std::vector<float>>{values}, ImageRepresentation::Image);
  const Image segmentation = makeImage(
    std::move(segmentationHeader),
    std::vector<std::vector<std::uint8_t>>{labels},
    ImageRepresentation::Segmentation);

  const auto result = computeRegionStatistics(image, segmentation, {}, 0);
  REQUIRE_FALSE(result);
  CHECK(result.error() == RegionStatisticsError::MismatchedSpatialGeometry);
}
