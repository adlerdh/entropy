#include "image/Image.h"
#include "image/Isosurface.h"
#include "image/ImageUtility.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <glm/glm.hpp>

#include <array>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <vector>

namespace
{
namespace fs = std::filesystem;

ComponentStats
makeStats(long double minValue, long double q25, long double median, long double q75, long double maxValue)
{
  ComponentStats stats;
  stats.onlineStats.min = minValue;
  stats.onlineStats.max = maxValue;
  stats.onlineStats.mean = 0.5L * (minValue + maxValue);
  stats.onlineStats.stdev = 2.0L;
  stats.onlineStats.variance = 4.0L;
  stats.onlineStats.sum = 0.0L;
  stats.onlineStats.count = 9;
  stats.quantiles.fill(minValue);
  stats.quantiles[1] = minValue;
  stats.quantiles[25] = q25;
  stats.quantiles[50] = median;
  stats.quantiles[75] = q75;
  stats.quantiles[99] = maxValue;
  stats.quantiles[100] = maxValue;
  return stats;
}

ImageIoInfo makeIoInfo(ComponentType componentType, uint32_t numComponents, glm::uvec3 dims)
{
  ImageIoInfo info;
  info.m_fileInfo.m_fileName = "synthetic.nrrd";
  info.m_fileInfo.m_fileTypeString = "Nrrd";
  info.m_componentInfo.m_componentType = componentType;
  info.m_componentInfo.m_componentTypeString = componentTypeString(componentType);
  info.m_componentInfo.m_componentSizeInBytes =
    static_cast<uint32_t>(componentRange(componentType).second == 255.0 ? 1 : 4);
  if (componentType == ComponentType::Int16 || componentType == ComponentType::UInt16) {
    info.m_componentInfo.m_componentSizeInBytes = 2;
  }

  info.m_pixelInfo.m_pixelType = numComponents == 1 ? PixelType::Scalar : PixelType::Vector;
  info.m_pixelInfo.m_pixelTypeString = numComponents == 1 ? "scalar" : "vector";
  info.m_pixelInfo.m_numComponents = numComponents;
  info.m_pixelInfo.m_pixelStrideInBytes = info.m_componentInfo.m_componentSizeInBytes * numComponents;

  info.m_sizeInfo.m_imageSizeInPixels = static_cast<std::size_t>(dims.x) * dims.y * dims.z;
  info.m_sizeInfo.m_imageSizeInComponents = info.m_sizeInfo.m_imageSizeInPixels * numComponents;
  info.m_sizeInfo.m_imageSizeInBytes =
    info.m_sizeInfo.m_imageSizeInComponents * info.m_componentInfo.m_componentSizeInBytes;

  info.m_spaceInfo.m_numDimensions = 3;
  info.m_spaceInfo.m_dimensions = {dims.x, dims.y, dims.z};
  info.m_spaceInfo.m_origin = {-1.0, 2.0, 3.0};
  info.m_spaceInfo.m_spacing = {0.5, 1.5, 2.5};
  info.m_spaceInfo.m_directions = {{1.0, 0.0, 0.0}, {0.0, 1.0, 0.0}, {0.0, 0.0, 1.0}};
  return info;
}

Image makeRawImage()
{
  const glm::uvec3 dims{2, 2, 1};
  ImageIoInfo ioInfo = makeIoInfo(ComponentType::UInt16, 1, dims);
  ImageHeader header(ioInfo, ioInfo, false);
  std::vector<uint16_t> values{1, 2, 3, 4};
  std::vector<const void*> buffers{values.data()};
  return Image(
    header,
    "raw",
    Image::ImageRepresentation::Image,
    Image::MultiComponentBufferType::SeparateImages,
    buffers);
}

fs::path testDirectory()
{
  const fs::path dir = fs::temp_directory_path() / "entropy-image-core-tests";
  fs::create_directories(dir);
  return dir;
}

} // namespace

TEST_CASE("Component ranges describe supported texture component types", "[image][utility]")
{
  CHECK(componentRange(ComponentType::Int8).first == -128.0);
  CHECK(componentRange(ComponentType::UInt8).second == 255.0);
  CHECK(componentRange(ComponentType::Int16).first == static_cast<double>(std::numeric_limits<int16_t>::lowest()));
  CHECK(componentRange(ComponentType::UInt16).second == static_cast<double>(std::numeric_limits<uint16_t>::max()));
  CHECK(componentRange(ComponentType::Undefined) == std::pair<double, double>{0.0, 0.0});
}

TEST_CASE("Image IO metadata validation rejects incomplete metadata", "[image][io-info]")
{
  ImageIoInfo info = makeIoInfo(ComponentType::UInt16, 1, glm::uvec3(2, 2, 1));

  REQUIRE(info.validate());

  ImageIoInfo invalidFile = info;
  invalidFile.m_fileInfo.m_fileName.clear();
  CHECK_FALSE(invalidFile.validate());

  ImageIoInfo invalidComponent = info;
  invalidComponent.m_componentInfo.m_componentType = ComponentType::Undefined;
  CHECK_FALSE(invalidComponent.validate());

  ImageIoInfo invalidPixel = info;
  invalidPixel.m_pixelInfo.m_numComponents = 0;
  CHECK_FALSE(invalidPixel.validate());

  ImageIoInfo invalidSize = info;
  invalidSize.m_sizeInfo.m_imageSizeInPixels = 0;
  CHECK_FALSE(invalidSize.validate());

  ImageIoInfo invalidSpace = info;
  invalidSpace.m_spaceInfo.m_numDimensions = 4;
  CHECK_FALSE(invalidSpace.validate());

  ImageIoInfo invalidDirectionRows = info;
  invalidDirectionRows.m_spaceInfo.m_directions.front().pop_back();
  CHECK_FALSE(invalidDirectionRows.validate());
}

TEST_CASE("Image IO metadata group validators reject their own incomplete state", "[image][io-info]")
{
  FileInfo fileInfo;
  CHECK_FALSE(fileInfo.validate());
  fileInfo.m_fileName = "valid.nrrd";
  CHECK(fileInfo.validate());

  ComponentInfo componentInfo;
  CHECK_FALSE(componentInfo.validate());
  componentInfo.m_componentType = ComponentType::UInt16;
  componentInfo.m_componentSizeInBytes = 2;
  CHECK(componentInfo.validate());

  PixelInfo pixelInfo;
  CHECK_FALSE(pixelInfo.validate());
  pixelInfo.m_pixelType = PixelType::Scalar;
  pixelInfo.m_numComponents = 1;
  pixelInfo.m_pixelStrideInBytes = 2;
  CHECK(pixelInfo.validate());

  SizeInfo sizeInfo;
  CHECK_FALSE(sizeInfo.validate());
  sizeInfo.m_imageSizeInPixels = 4;
  sizeInfo.m_imageSizeInComponents = 4;
  sizeInfo.m_imageSizeInBytes = 8;
  CHECK(sizeInfo.validate());

  SpaceInfo spaceInfo;
  CHECK_FALSE(spaceInfo.validate());
  spaceInfo.m_numDimensions = 2;
  spaceInfo.m_dimensions = {2, 3};
  spaceInfo.m_origin = {0.0, 1.0};
  spaceInfo.m_spacing = {0.5, 1.5};
  spaceInfo.m_directions = {{1.0, 0.0}, {0.0, 1.0}};
  CHECK(spaceInfo.validate());

  spaceInfo.m_directions.back().push_back(0.0);
  CHECK_FALSE(spaceInfo.validate());
}

TEST_CASE("Isosurface material helpers compute lighting colors", "[image][isosurface]")
{
  Isosurface surface;
  surface.color = glm::vec3(0.2f, 0.4f, 0.8f);
  surface.material.ambient = 0.25f;
  surface.material.diffuse = 0.5f;
  surface.material.specular = 0.75f;

  CHECK(surface.ambientColor().x == Catch::Approx(0.05f));
  CHECK(surface.ambientColor().y == Catch::Approx(0.1f));
  CHECK(surface.ambientColor().z == Catch::Approx(0.2f));
  CHECK(surface.diffuseColor().x == Catch::Approx(0.1f));
  CHECK(surface.diffuseColor().y == Catch::Approx(0.2f));
  CHECK(surface.diffuseColor().z == Catch::Approx(0.4f));
  CHECK(surface.specularColor().x == Catch::Approx(0.75f));
  CHECK(surface.specularColor().y == Catch::Approx(0.75f));
  CHECK(surface.specularColor().z == Catch::Approx(0.75f));
}

TEST_CASE("Histogram bin rules handle normal and degenerate statistics", "[image][utility][histogram]")
{
  const ComponentStats stats = makeStats(0.0, 2.0, 4.0, 6.0, 8.0);

  CHECK(computeNumHistogramBins(NumBinsComputationMethod::SquareRoot, 16, stats).value() == 4);
  CHECK(computeNumHistogramBins(NumBinsComputationMethod::Sturges, 16, stats).value() == 5);
  CHECK(computeNumHistogramBins(NumBinsComputationMethod::Rice, 8, stats).value() == 4);
  CHECK(computeNumHistogramBins(NumBinsComputationMethod::Scott, 8, stats).has_value());
  CHECK(computeNumHistogramBins(NumBinsComputationMethod::FreedmanDiaconis, 8, stats).has_value());
  CHECK_FALSE(computeNumHistogramBins(NumBinsComputationMethod::SquareRoot, 0, stats).has_value());

  ComponentStats degenerate = stats;
  degenerate.onlineStats.stdev = 0.0;
  degenerate.quantiles[25] = 4.0;
  degenerate.quantiles[75] = 4.0;
  CHECK_FALSE(computeNumHistogramBins(NumBinsComputationMethod::Scott, 8, degenerate).has_value());
  CHECK_FALSE(computeNumHistogramBins(NumBinsComputationMethod::FreedmanDiaconis, 8, degenerate).has_value());
}

TEST_CASE("ImageSettings initializes component ranges, windows, and histogram settings from stats", "[image][settings]")
{
  const ComponentStats stats = makeStats(0.0, 2.0, 4.0, 6.0, 8.0);
  ImageSettings settings("settings", 9, 1, ComponentType::UInt16, {stats});

  CHECK(settings.displayName() == "settings");
  CHECK(settings.numComponents() == 1);
  CHECK(settings.componentType() == ComponentType::UInt16);
  CHECK(settings.componentStatistics(0).onlineStats.count == 9);
  CHECK(settings.minMaxImageRange(0) == std::pair<double, double>{0.0, 8.0});
  CHECK(settings.windowValuesLowHigh(0).first == Catch::Approx(0.0));
  CHECK(settings.windowValuesLowHigh(0).second == Catch::Approx(8.0));
  CHECK(settings.foregroundThresholds(0).first == Catch::Approx(4.0));
  CHECK(settings.histogramSettings(0).m_intensityRange[0] == Catch::Approx(0.0));
  CHECK(settings.histogramSettings(0).m_intensityRange[1] == Catch::Approx(8.0));
}

TEST_CASE("Raw Image construction computes values, stats, and transformations", "[image][raw]")
{
  Image image = makeRawImage();

  CHECK(image.hasPixelData());
  CHECK(image.header().existsOnDisk());
  CHECK(image.header().pixelDimensions() == glm::uvec3(2, 2, 1));
  CHECK(image.value<double>(0, 0, 0, 0).value() == Catch::Approx(1.0));
  CHECK(image.value<double>(0, 1, 1, 0).value() == Catch::Approx(4.0));
  CHECK_FALSE(image.value<double>(0, 2, 0, 0).has_value());
  CHECK(image.valueLinear<double>(0, 0.5, 0.0, 0.0).value() == Catch::Approx(1.5));

  CHECK(image.settings().componentStatistics(0).onlineStats.min == Catch::Approx(1.0));
  CHECK(image.settings().componentStatistics(0).onlineStats.max == Catch::Approx(4.0));

  const glm::vec4 subject = image.transformations().subject_T_pixel() * glm::vec4(1.0f, 1.0f, 0.0f, 1.0f);
  CHECK(subject.x == Catch::Approx(-0.5f));
  CHECK(subject.y == Catch::Approx(3.5f));
  CHECK(subject.z == Catch::Approx(3.0f));

  image.setUseIdentityPixelSpacings(true);
  CHECK(image.header().spacing() == glm::vec3(1.0f));
  image.setUseZeroPixelOrigin(true);
  CHECK(image.header().origin() == glm::vec3(0.0f));
}

TEST_CASE("Raw interleaved images truncate to the first four components per pixel", "[image][raw][interleaved]")
{
  const glm::uvec3 dims{2, 1, 1};
  ImageIoInfo ioInfo = makeIoInfo(ComponentType::UInt16, 5, dims);
  ImageHeader header(ioInfo, ioInfo, true);

  const std::vector<uint16_t> interleavedValues{10, 11, 12, 13, 14, 20, 21, 22, 23, 24};
  std::vector<const void*> buffers{interleavedValues.data()};

  Image image(
    header,
    "raw-interleaved",
    Image::ImageRepresentation::Image,
    Image::MultiComponentBufferType::InterleavedImage,
    buffers);

  CHECK(image.header().numComponentsPerPixel() == 4);
  CHECK(image.value<double>(0, 0).value() == Catch::Approx(10.0));
  CHECK(image.value<double>(1, 0).value() == Catch::Approx(11.0));
  CHECK(image.value<double>(2, 0).value() == Catch::Approx(12.0));
  CHECK(image.value<double>(3, 0).value() == Catch::Approx(13.0));
  CHECK(image.value<double>(0, 1).value() == Catch::Approx(20.0));
  CHECK(image.value<double>(1, 1).value() == Catch::Approx(21.0));
  CHECK(image.value<double>(2, 1).value() == Catch::Approx(22.0));
  CHECK(image.value<double>(3, 1).value() == Catch::Approx(23.0));

  CHECK(image.generateSortedBuffers());
  CHECK(image.quantileToValue(0, 1.0) == Catch::Approx(20.0));
  CHECK(image.quantileToValue(3, 1.0) == Catch::Approx(23.0));
}

TEST_CASE("Interleaved image components save as scalar images", "[image][save][interleaved]")
{
  const glm::uvec3 dims{2, 1, 1};
  ImageIoInfo ioInfo = makeIoInfo(ComponentType::UInt16, 3, dims);
  ImageHeader header(ioInfo, ioInfo, true);

  const std::vector<uint16_t> interleavedValues{10, 11, 12, 20, 21, 22};
  std::vector<const void*> buffers{interleavedValues.data()};

  Image image(
    header,
    "raw-interleaved-save",
    Image::ImageRepresentation::Image,
    Image::MultiComponentBufferType::InterleavedImage,
    buffers);

  const fs::path fileName = testDirectory() / "saved-interleaved-component.nrrd";
  REQUIRE(image.saveComponentToDisk(1, fileName));

  Image reloaded(fileName, Image::ImageRepresentation::Image, Image::MultiComponentBufferType::SeparateImages);
  CHECK(reloaded.header().numComponentsPerPixel() == 1);
  CHECK(reloaded.header().memoryComponentType() == ComponentType::UInt16);
  CHECK(reloaded.value<double>(0, 0).value() == Catch::Approx(11.0));
  CHECK(reloaded.value<double>(0, 1).value() == Catch::Approx(21.0));
}

TEST_CASE("Image components save to disk and read back with geometry and values intact", "[image][save][io]")
{
  Image image = makeRawImage();
  const fs::path fileName = testDirectory() / "saved-component.nrrd";

  REQUIRE(image.saveComponentToDisk(0, fileName));

  Image reloaded(fileName, Image::ImageRepresentation::Image, Image::MultiComponentBufferType::SeparateImages);
  CHECK(reloaded.header().pixelDimensions() == image.header().pixelDimensions());
  CHECK(reloaded.header().spacing() == image.header().spacing());
  CHECK(reloaded.header().origin() == image.header().origin());
  CHECK(reloaded.header().memoryComponentType() == ComponentType::UInt16);
  CHECK(reloaded.value<double>(0, 0).value() == Catch::Approx(1.0));
  CHECK(reloaded.value<double>(0, 3).value() == Catch::Approx(4.0));

  CHECK_FALSE(image.saveComponentToDisk(1, testDirectory() / "missing-component.nrrd"));
}
