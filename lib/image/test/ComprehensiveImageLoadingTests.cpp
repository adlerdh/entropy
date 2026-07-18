#include "ImageGenerator.h"
#include "image/Image.h"
#include "image/ImageUtility.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <glm/gtc/epsilon.hpp>
#include <glm/vec3.hpp>

#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace
{
namespace fs = std::filesystem;

using image_generator::ImageSpec;
using image_generator::Pattern;
using image_generator::PixelKind;

using BufferType = Image::MultiComponentBufferType;
using Rep = Image::ImageRepresentation;

struct LoadingCase
{
  std::string name;
  std::string extension = ".nrrd";
  PixelKind pixelKind = PixelKind::Scalar;
  std::string componentType = "float";
  std::size_t components = 1;
  std::vector<std::size_t> size;
  std::vector<double> spacing;
  std::vector<double> origin;
  std::vector<double> direction;
  Pattern pattern = Pattern::Ramp;
  double amplitude = 10.0;
  double offset = 0.0;
  BufferType bufferType = BufferType::SeparateImages;
  bool entropyTimeAxis = false;
  std::string timeUnits = "frame";
  bool expectOblique = false;
};

fs::path testDirectory()
{
  const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
  fs::path dir = fs::temp_directory_path() / ("entropy-comprehensive-image-loading-" + std::to_string(stamp));
  fs::create_directories(dir);
  return dir;
}

std::vector<double> identityDirection(std::size_t dimensions)
{
  std::vector<double> direction(dimensions * dimensions, 0.0);
  for (std::size_t i = 0; i < dimensions; ++i) {
    direction[(i * dimensions) + i] = 1.0;
  }
  return direction;
}

std::vector<double> defaultSpacing(std::size_t dimensions)
{
  std::vector<double> spacing(dimensions, 1.0);
  for (std::size_t i = 0; i < dimensions; ++i) {
    spacing[i] = 0.5 + static_cast<double>(i + 1) * 0.375;
  }
  return spacing;
}

std::vector<double> defaultOrigin(std::size_t dimensions)
{
  std::vector<double> origin(dimensions, 0.0);
  for (std::size_t i = 0; i < dimensions; ++i) {
    origin[i] = -2.0 + static_cast<double>(i) * 1.25;
  }
  return origin;
}

ImageSpec makeSpec(const LoadingCase& testCase, const fs::path& dir)
{
  ImageSpec spec;
  spec.output = dir / (testCase.name + testCase.extension);
  spec.pixelKind = testCase.pixelKind;
  spec.componentType = testCase.componentType;
  spec.components = testCase.components;
  spec.size = testCase.size;
  spec.spacing = testCase.spacing.empty() ? defaultSpacing(testCase.size.size()) : testCase.spacing;
  spec.origin = testCase.origin.empty() ? defaultOrigin(testCase.size.size()) : testCase.origin;
  spec.direction = testCase.direction.empty() ? identityDirection(testCase.size.size()) : testCase.direction;
  spec.pattern = testCase.pattern;
  spec.amplitude = testCase.amplitude;
  spec.offset = testCase.offset;
  if (testCase.entropyTimeAxis) {
    spec.metadata["entropy_time_axis"] = "last";
    spec.metadata["entropy_time_units"] = testCase.timeUnits;
  }
  spec.metadata["entropy_test_case"] = testCase.name;
  return spec;
}

bool hasEntropyTimeAxis(const ImageSpec& spec)
{
  const auto it = spec.metadata.find("entropy_time_axis");
  return it != spec.metadata.end() && it->second == "last";
}

bool hasTimeAxis(const ImageSpec& spec)
{
  return spec.size.size() == 4 || (hasEntropyTimeAxis(spec) && spec.size.size() >= 2);
}

std::size_t spatialDimensionCount(const ImageSpec& spec)
{
  return hasTimeAxis(spec) ? spec.size.size() - 1 : spec.size.size();
}

uint32_t expectedTimePoints(const ImageSpec& spec)
{
  return hasTimeAxis(spec) ? static_cast<uint32_t>(spec.size.back()) : 1u;
}

std::string expectedTimeUnits(const ImageSpec& spec)
{
  if (!hasTimeAxis(spec)) {
    return "frame";
  }

  const auto it = spec.metadata.find("entropy_time_units");
  return it != spec.metadata.end() ? it->second : "frame";
}

glm::uvec3 expectedPixelDimensions(const ImageSpec& spec)
{
  glm::uvec3 dims{1u};
  const std::size_t spatialDims = spatialDimensionCount(spec);
  for (std::size_t axis = 0; axis < spatialDims && axis < 3; ++axis) {
    dims[axis] = static_cast<uint32_t>(spec.size.at(axis));
  }
  return dims;
}

glm::vec3 expectedSpacing(const ImageSpec& spec)
{
  glm::vec3 spacing{1.0f};
  const std::size_t spatialDims = spatialDimensionCount(spec);
  for (std::size_t axis = 0; axis < spatialDims && axis < 3; ++axis) {
    spacing[axis] = static_cast<float>(spec.spacing.at(axis));
  }
  return spacing;
}

glm::vec3 expectedOrigin(const ImageSpec& spec)
{
  glm::vec3 origin{0.0f};
  const std::size_t spatialDims = spatialDimensionCount(spec);
  for (std::size_t axis = 0; axis < spatialDims && axis < 3; ++axis) {
    origin[axis] = static_cast<float>(spec.origin.at(axis));
  }
  return origin;
}

uint32_t expectedComponents(const LoadingCase& testCase)
{
  if (testCase.pixelKind == PixelKind::Scalar) {
    return 1u;
  }
  if (testCase.pixelKind == PixelKind::Complex) {
    return 2u;
  }
  if (testCase.pixelKind == PixelKind::RGB) {
    return 3u;
  }
  if (testCase.pixelKind == PixelKind::RGBA) {
    return 4u;
  }
  return static_cast<uint32_t>(testCase.components);
}

ComponentType expectedMemoryComponentType(const LoadingCase& testCase)
{
  const std::string& type = testCase.componentType;
  if (type == "int8" || type == "char") {
    return ComponentType::Int8;
  }
  if (type == "uint8" || type == "uchar") {
    return ComponentType::UInt8;
  }
  if (type == "int16" || type == "short") {
    return ComponentType::Int16;
  }
  if (type == "uint16" || type == "ushort") {
    return ComponentType::UInt16;
  }
  if (type == "int32" || type == "int") {
    return ComponentType::Int32;
  }
  if (type == "uint32" || type == "uint") {
    return ComponentType::UInt32;
  }
  return ComponentType::Float32;
}

double expectedValueAfterFileCast(const LoadingCase& testCase, double value)
{
  const std::string& type = testCase.componentType;
  if (type == "int8" || type == "char") {
    return static_cast<double>(static_cast<int8_t>(value));
  }
  if (type == "uint8" || type == "uchar") {
    return static_cast<double>(static_cast<uint8_t>(value));
  }
  if (type == "int16" || type == "short") {
    return static_cast<double>(static_cast<int16_t>(value));
  }
  if (type == "uint16" || type == "ushort") {
    return static_cast<double>(static_cast<uint16_t>(value));
  }
  if (type == "int32" || type == "int") {
    return static_cast<double>(static_cast<int32_t>(value));
  }
  if (type == "uint32" || type == "uint") {
    return static_cast<double>(static_cast<uint32_t>(value));
  }
  if (type == "float" || type == "float32") {
    return static_cast<double>(static_cast<float>(value));
  }
  return value;
}

std::vector<std::size_t> expectedIndex(const ImageSpec& spec, const glm::uvec3& spatialIndex, uint32_t timePoint)
{
  std::vector<std::size_t> index(spec.size.size(), 0u);
  const std::size_t spatialDims = spatialDimensionCount(spec);
  for (std::size_t axis = 0; axis < spatialDims && axis < 3; ++axis) {
    index[axis] = spatialIndex[axis];
  }
  if (hasTimeAxis(spec)) {
    index.back() = timePoint;
  }
  return index;
}

double expectedValue(
  const LoadingCase& testCase,
  const ImageSpec& spec,
  const glm::uvec3& spatialIndex,
  uint32_t timePoint,
  uint32_t component)
{
  const std::vector<std::size_t> index = expectedIndex(spec, spatialIndex, timePoint);
  if (spec.pixelKind == PixelKind::Complex) {
    const auto value = image_generator::expectedComplexValue(spec, index);
    return expectedValueAfterFileCast(testCase, component == 0u ? value.real() : value.imag());
  }
  return expectedValueAfterFileCast(testCase, image_generator::expectedComponentValue(spec, index, component));
}

std::vector<glm::uvec3> sampleIndices(const ImageSpec& spec)
{
  const glm::uvec3 dims = expectedPixelDimensions(spec);
  std::vector<glm::uvec3> samples{{0u, 0u, 0u}};
  samples.emplace_back(dims.x - 1u, dims.y - 1u, dims.z - 1u);
  samples.emplace_back(dims.x / 2u, dims.y / 2u, dims.z / 2u);
  return samples;
}

void checkVectorNear(const glm::vec3& actual, const glm::vec3& expected)
{
  CHECK(actual.x == Catch::Approx(expected.x));
  CHECK(actual.y == Catch::Approx(expected.y));
  CHECK(actual.z == Catch::Approx(expected.z));
}

void checkLoadedImage(const LoadingCase& testCase, const ImageSpec& spec)
{
  std::string phase = "read header";
  CAPTURE(phase);
  const auto headerOnly = readImageHeaderOnly(spec.output, Rep::Image, testCase.bufferType);
  REQUIRE(headerOnly);
  CHECK(headerOnly->pixelDimensions() == expectedPixelDimensions(spec));
  CHECK(headerOnly->numComponentsPerPixel() == expectedComponents(testCase));

  phase = "construct image";
  CAPTURE(phase);
  Image image(spec.output, Rep::Image, testCase.bufferType);

  phase = "check metadata";
  CAPTURE(phase);
  CHECK(image.loadState() == ImageLoadState::LoadedPixels);
  CHECK(image.hasPixelData());
  CHECK(image.header().pixelDimensions() == expectedPixelDimensions(spec));
  CHECK(image.header().numComponentsPerPixel() == expectedComponents(testCase));
  CHECK(image.header().memoryComponentType() == expectedMemoryComponentType(testCase));
  CHECK(image.header().interleavedComponents() == (testCase.bufferType == BufferType::InterleavedImage));
  CHECK(image.timeAxis().numTimePoints() == expectedTimePoints(spec));
  CHECK(image.timeAxis().units() == expectedTimeUnits(spec));
  CHECK(image.isTimeSeries() == (expectedTimePoints(spec) > 1u));
  CHECK(image.header().isOblique() == testCase.expectOblique);
  checkVectorNear(image.header().spacing(), expectedSpacing(spec));
  checkVectorNear(image.header().origin(), expectedOrigin(spec));

  if (image.timeAxis().isTimeSeries() && image.timeAxis().units() == "sec") {
    CHECK(image.timeAxis().playbackFramePeriodSeconds(1.0) == Catch::Approx(std::abs(spec.spacing.back())));
  }

  const uint32_t lastTimePoint = image.timeAxis().numTimePoints() - 1u;
  const std::vector<uint32_t> timePoints =
    lastTimePoint == 0u ? std::vector<uint32_t>{0u} : std::vector<uint32_t>{0u, lastTimePoint};

  for (const uint32_t timePoint : timePoints) {
    for (const glm::uvec3& sample : sampleIndices(spec)) {
      for (uint32_t component = 0; component < image.header().numComponentsPerPixel(); ++component) {
        phase = "check values";
        CAPTURE(phase);
        const auto actual = image.value<double>(component, sample.x, sample.y, sample.z, timePoint);
        REQUIRE(actual);
        const double expected = expectedValue(testCase, spec, sample, timePoint, component);
        CHECK(*actual == Catch::Approx(expected).epsilon(1.0e-5).margin(1.0e-4));
      }
    }
  }
}

std::vector<LoadingCase> comprehensiveCases()
{
  const double angle = 0.30;
  const std::vector<double>
    oblique3d{std::cos(angle), -std::sin(angle), 0.0, std::sin(angle), std::cos(angle), 0.0, 0.0, 0.0, 1.0};

  return {
    {.name = "scalar-1d-int8-nrrd",
     .componentType = "int8",
     .size = {8},
     .spacing = {0.35},
     .origin = {-1.25},
     .pattern = Pattern::Ramp,
     .amplitude = 9.0,
     .offset = -5.0},
    {.name = "scalar-1d-uint8-nrrd",
     .componentType = "uint8",
     .size = {7},
     .spacing = {0.4},
     .origin = {-1.5},
     .pattern = Pattern::Ramp,
     .amplitude = 12.0,
     .offset = 20.0},
    {.name = "scalar-2d-int16-mha",
     .extension = ".mha",
     .componentType = "int16",
     .size = {5, 4},
     .spacing = {0.7, 1.4},
     .origin = {-2.0, 3.0},
     .pattern = Pattern::Checker,
     .amplitude = 8.0,
     .offset = 4.0},
    {.name = "scalar-2d-uint32-nrrd",
     .componentType = "uint32",
     .size = {4, 5},
     .spacing = {0.9, 1.3},
     .origin = {1.0, -2.5},
     .pattern = Pattern::Ramp,
     .amplitude = 10.0,
     .offset = 30.0},
    {.name = "scalar-3d-float-nii",
     .extension = ".nii",
     .componentType = "float",
     .size = {4, 3, 2},
     .spacing = {0.8, 1.1, 2.2},
     .origin = {-4.0, -3.0, 2.0},
     .pattern = Pattern::Gaussian,
     .amplitude = 25.0,
     .offset = 2.0},
    {.name = "scalar-3d-int32-nrrd",
     .componentType = "int32",
     .size = {3, 4, 2},
     .spacing = {0.6, 1.1, 2.4},
     .origin = {-3.0, 1.5, 2.5},
     .pattern = Pattern::Ramp,
     .amplitude = 7.0,
     .offset = -20.0},
    {.name = "scalar-1d-time-float-nrrd",
     .componentType = "float",
     .size = {6, 4},
     .spacing = {0.5, 1.25},
     .origin = {-1.0, 10.0},
     .pattern = Pattern::TimeRamp,
     .amplitude = 3.0,
     .entropyTimeAxis = true},
    {.name = "scalar-1d-time-canonical-mha",
     .extension = ".mha",
     .componentType = "float",
     .size = {6, 1, 1, 4},
     .spacing = {0.5, 1.0, 1.0, 2.0},
     .origin = {-1.0, 0.0, 0.0, 10.0},
     .pattern = Pattern::TimeRamp,
     .amplitude = 4.0},
    {.name = "scalar-2d-time-double-nrrd",
     .componentType = "double",
     .size = {5, 4, 3},
     .spacing = {0.5, 0.75, 2.5},
     .origin = {-2.0, -3.0, 5.0},
     .pattern = Pattern::TemporalSine,
     .amplitude = 11.0,
     .entropyTimeAxis = true},
    {.name = "scalar-2d-time-canonical-nii",
     .extension = ".nii",
     .componentType = "float",
     .size = {5, 4, 1, 3},
     .spacing = {0.5, 0.75, 1.0, 2.5},
     .origin = {-2.0, -3.0, 0.0, 5.0},
     .pattern = Pattern::MovingGaussian,
     .amplitude = 9.0},
    {.name = "scalar-3d-time-mha",
     .extension = ".mha",
     .componentType = "float",
     .size = {4, 3, 2, 3},
     .spacing = {0.8, 1.0, 2.0, 0.2},
     .origin = {-2.0, -1.0, 4.0, 0.0},
     .pattern = Pattern::TimeRamp,
     .amplitude = 5.0,
     .entropyTimeAxis = true,
     .timeUnits = "sec"},
    {.name = "vector-2comp-2d-uint16-nrrd",
     .pixelKind = PixelKind::Vector,
     .componentType = "uint16",
     .components = 2,
     .size = {5, 3},
     .pattern = Pattern::ComponentRamp,
     .offset = 10.0},
    {.name = "vector-3comp-3d-time-mhd-interleaved",
     .extension = ".mhd",
     .pixelKind = PixelKind::Vector,
     .componentType = "float",
     .components = 3,
     .size = {4, 3, 2, 3},
     .spacing = {0.9, 1.2, 1.7, 1.0},
     .pattern = Pattern::MovingGaussian,
     .amplitude = 13.0,
     .bufferType = BufferType::InterleavedImage},
    {.name = "vector-4comp-2d-time-nrrd",
     .pixelKind = PixelKind::Vector,
     .componentType = "float",
     .components = 4,
     .size = {4, 3, 3},
     .pattern = Pattern::TimeRamp,
     .entropyTimeAxis = true},
    {.name = "rgb-3comp-3d-uint8-nrrd",
     .pixelKind = PixelKind::RGB,
     .componentType = "uint8",
     .components = 3,
     .size = {4, 3, 2},
     .pattern = Pattern::Ramp,
     .amplitude = 20.0,
     .offset = 20.0},
    {.name = "rgba-4comp-3d-uint8-nrrd",
     .pixelKind = PixelKind::RGBA,
     .componentType = "uint8",
     .components = 4,
     .size = {4, 3, 2},
     .pattern = Pattern::Ramp,
     .amplitude = 18.0,
     .offset = 30.0},
    {.name = "vector-5comp-1d-time-nrrd-interleaved",
     .pixelKind = PixelKind::Vector,
     .componentType = "float",
     .components = 5,
     .size = {5, 3},
     .pattern = Pattern::ComponentRamp,
     .bufferType = BufferType::InterleavedImage,
     .entropyTimeAxis = true},
    {.name = "complex-3d-float-nrrd",
     .pixelKind = PixelKind::Complex,
     .componentType = "float",
     .size = {4, 3, 2},
     .pattern = Pattern::ComplexPhase,
     .amplitude = 7.0},
    {.name = "complex-2d-time-nrrd",
     .pixelKind = PixelKind::Complex,
     .componentType = "float",
     .size = {4, 3, 3},
     .pattern = Pattern::ComplexPhase,
     .amplitude = 6.0,
     .entropyTimeAxis = true},
    {.name = "vector-3comp-oblique-3d-nrrd",
     .pixelKind = PixelKind::Vector,
     .componentType = "float",
     .components = 3,
     .size = {4, 3, 2},
     .direction = oblique3d,
     .pattern = Pattern::ComponentRamp,
     .expectOblique = true},
  };
}
} // namespace

TEST_CASE("Generated synthetic images round-trip through Entropy image loading", "[image][loading][comprehensive]")
{
  const fs::path dir = testDirectory();

  for (const LoadingCase& testCase : comprehensiveCases()) {
    CAPTURE(testCase.name);
    const ImageSpec spec = makeSpec(testCase, dir);
    image_generator::writeImage(spec);
    REQUIRE(fs::exists(spec.output));
    checkLoadedImage(testCase, spec);

    LoadingCase alternateLayoutCase = testCase;
    alternateLayoutCase.bufferType =
      testCase.bufferType == BufferType::SeparateImages ? BufferType::InterleavedImage : BufferType::SeparateImages;
    CAPTURE(alternateLayoutCase.bufferType);
    checkLoadedImage(alternateLayoutCase, spec);
  }
}
