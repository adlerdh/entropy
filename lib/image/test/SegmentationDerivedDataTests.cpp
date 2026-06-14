#include "image/ImageDerivedData.h"
#include "image/SegUtil.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <glm/glm.hpp>

#include <cstdint>
#include <vector>

namespace
{

ImageIoInfo makeIoInfo(ComponentType componentType, uint32_t numComponents, glm::uvec3 dims)
{
  ImageIoInfo info;
  info.m_fileInfo.m_fileName = "seg-derived.nrrd";
  info.m_fileInfo.m_fileTypeString = "Nrrd";
  info.m_componentInfo.m_componentType = componentType;
  info.m_componentInfo.m_componentTypeString = componentTypeString(componentType);
  info.m_componentInfo.m_componentSizeInBytes = componentType == ComponentType::UInt16 ? 2 : 4;

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
  info.m_spaceInfo.m_origin = {0.0, 0.0, 0.0};
  info.m_spaceInfo.m_spacing = {1.0, 1.0, 1.0};
  info.m_spaceInfo.m_directions = {{1.0, 0.0, 0.0}, {0.0, 1.0, 0.0}, {0.0, 0.0, 1.0}};
  return info;
}

Image makeSegmentation()
{
  const glm::uvec3 dims{5, 5, 5};
  ImageIoInfo ioInfo = makeIoInfo(ComponentType::UInt16, 1, dims);
  ImageHeader header(ioInfo, ioInfo, false);
  std::vector<uint16_t> labels(static_cast<std::size_t>(dims.x) * dims.y * dims.z, 0);
  labels[2 + dims.x * (2 + dims.y * 2)] = 1;
  std::vector<const void*> buffers{labels.data()};
  return Image(
    header,
    "seg",
    Image::ImageRepresentation::Segmentation,
    Image::MultiComponentBufferType::SeparateImages,
    buffers);
}

Image makeScalarImage()
{
  const glm::uvec3 dims{4, 4, 4};
  ImageIoInfo ioInfo = makeIoInfo(ComponentType::Float32, 1, dims);
  ImageHeader header(ioInfo, ioInfo, false);
  std::vector<float> values(static_cast<std::size_t>(dims.x) * dims.y * dims.z, 0.0f);
  for (std::size_t i = 0; i < values.size(); ++i) {
    values[i] = static_cast<float>(i % 7);
  }
  std::vector<const void*> buffers{values.data()};
  return Image(
    header,
    "image",
    Image::ImageRepresentation::Image,
    Image::MultiComponentBufferType::SeparateImages,
    buffers);
}

Image makeInterleavedImage()
{
  const glm::uvec3 dims{2, 2, 1};
  ImageIoInfo ioInfo = makeIoInfo(ComponentType::UInt16, 3, dims);
  ImageHeader header(ioInfo, ioInfo, true);
  std::vector<uint16_t> values{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};
  std::vector<const void*> buffers{values.data()};
  return Image(
    header,
    "interleaved",
    Image::ImageRepresentation::Image,
    Image::MultiComponentBufferType::InterleavedImage,
    buffers);
}

} // namespace

TEST_CASE("Segmentation brush footprints respect 3D shape, bounds, and isotropic spacing", "[image][segmentation]")
{
  Image seg = makeSegmentation();
  const glm::vec4 unusedPlane{0.0f, 0.0f, 1.0f, -2.0f};

  const auto cube = computeSegmentationBrushFootprint(seg, false, true, false, 2, glm::ivec3(2, 2, 2), unusedPlane);
  CHECK(cube.voxels.size() == 27);
  CHECK(cube.minVoxel == glm::ivec3(1, 1, 1));
  CHECK(cube.maxVoxel == glm::ivec3(3, 3, 3));

  const auto sphere = computeSegmentationBrushFootprint(seg, true, true, false, 2, glm::ivec3(2, 2, 2), unusedPlane);
  CHECK(sphere.voxels.size() == 7);
  CHECK(sphere.voxels.count(glm::ivec3(2, 2, 2)) == 1);
  CHECK(sphere.voxels.count(glm::ivec3(3, 3, 2)) == 0);

  const auto clipped = computeSegmentationBrushFootprint(seg, false, true, false, 2, glm::ivec3(0, 0, 0), unusedPlane);
  CHECK(clipped.voxels.size() == 8);
  CHECK(clipped.minVoxel == glm::ivec3(0, 0, 0));
  CHECK(clipped.maxVoxel == glm::ivec3(1, 1, 1));
}

TEST_CASE("Painting segmentation updates labels and reports the changed texture block", "[image][segmentation]")
{
  Image seg = makeSegmentation();
  const glm::vec4 unusedPlane{0.0f, 0.0f, 1.0f, -2.0f};

  bool callbackCalled = false;
  glm::uvec3 callbackOffset{0};
  glm::uvec3 callbackSize{0};
  std::vector<int64_t> callbackValues;

  paintSegmentation(
    seg,
    5,
    0,
    false,
    true,
    true,
    false,
    2,
    glm::ivec3(2, 2, 2),
    unusedPlane,
    std::nullopt,
    [&](
      const ComponentType& memoryComponentType,
      const glm::uvec3& offset,
      const glm::uvec3& size,
      const int64_t* data) {
      callbackCalled = true;
      CHECK(memoryComponentType == ComponentType::UInt16);
      callbackOffset = offset;
      callbackSize = size;
      callbackValues.assign(data, data + static_cast<std::size_t>(size.x) * size.y * size.z);
    });

  CHECK(callbackCalled);
  CHECK(callbackOffset == glm::uvec3(1, 1, 1));
  CHECK(callbackSize == glm::uvec3(3, 3, 3));
  CHECK(callbackValues.size() == 27);
  CHECK(seg.value<int64_t>(0, 2, 2, 2).value() == 5);
  CHECK(seg.value<int64_t>(0, 3, 3, 2).value() == 0);

  paintSegmentation(
    seg,
    9,
    0,
    true,
    true,
    true,
    false,
    1,
    glm::ivec3(2, 2, 2),
    unusedPlane,
    std::nullopt,
    [](const ComponentType&, const glm::uvec3&, const glm::uvec3&, const int64_t*) {});
  CHECK(seg.value<int64_t>(0, 2, 2, 2).value() == 5);
}

TEST_CASE("ImageDerivedData creates derived images and skips unsupported interleaved distance maps", "[image][derived]")
{
  Image image = makeScalarImage();

  const auto noise = createNoiseEstimateImages(image, 1);
  REQUIRE(noise.size() == 1);
  CHECK(noise.front().component == 0);
  CHECK(noise.front().image.header().pixelDimensions() == image.header().pixelDimensions());
  CHECK(noise.front().image.header().memoryComponentType() == ComponentType::Float32);

  image.settings().setForegroundThresholdLow(0, 1.0);
  image.settings().setForegroundThresholdHigh(0, 6.0);
  const auto distanceMaps = createDistanceMapImages(image, 1.0f);
  REQUIRE(distanceMaps.size() == 1);
  CHECK(distanceMaps.front().component == 0);
  CHECK(distanceMaps.front().boundaryIsoValue == Catch::Approx(6.0));
  CHECK(distanceMaps.front().image.header().pixelDimensions() == image.header().pixelDimensions());
  CHECK(distanceMaps.front().image.header().memoryComponentType() == ComponentType::UInt8);

  const auto skipped = createDistanceMapImages(makeInterleavedImage(), 1.0f);
  CHECK(skipped.empty());
}
