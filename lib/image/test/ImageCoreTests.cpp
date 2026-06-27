#include "image/Image.h"
#include "image/ImageDerivedData.h"
#include "image/Isosurface.h"
#include "image/ImageUtility.h"
#include "image/internal/ImageCastHelper.tpp"
#include "image/internal/ImageUtilityItk.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <glm/glm.hpp>

#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <limits>
#include <numbers>
#include <stdexcept>
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

uint32_t componentSizeInBytes(ComponentType componentType)
{
  switch (componentType) {
    case ComponentType::Int8:
    case ComponentType::UInt8:
      return 1;
    case ComponentType::Int16:
    case ComponentType::UInt16:
      return 2;
    case ComponentType::Int32:
    case ComponentType::UInt32:
    case ComponentType::Float32:
      return 4;
    default:
      return 0;
  }
}

ImageIoInfo makeIoInfo(ComponentType componentType, uint32_t numComponents, glm::uvec3 dims)
{
  ImageIoInfo info;
  info.m_fileInfo.m_fileName = "synthetic.nrrd";
  info.m_fileInfo.m_fileTypeString = "Nrrd";
  info.m_componentInfo.m_componentType = componentType;
  info.m_componentInfo.m_componentTypeString = componentTypeString(componentType);
  info.m_componentInfo.m_componentSizeInBytes = componentSizeInBytes(componentType);

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

Image makeSparseBinaryImage()
{
  const glm::uvec3 dims{100, 100, 1};
  ImageIoInfo ioInfo = makeIoInfo(ComponentType::Float32, 1, dims);
  ioInfo.m_sizeInfo.m_imageSizeInPixels = static_cast<std::size_t>(dims.x) * dims.y * dims.z;
  ioInfo.m_sizeInfo.m_imageSizeInComponents = ioInfo.m_sizeInfo.m_imageSizeInPixels;
  ioInfo.m_sizeInfo.m_imageSizeInBytes = ioInfo.m_sizeInfo.m_imageSizeInComponents * sizeof(float);
  ImageHeader header(ioInfo, ioInfo, false);

  std::vector<float> values(ioInfo.m_sizeInfo.m_imageSizeInPixels, 0.0f);
  values.at(values.size() / 2) = 100.0f;
  std::vector<const void*> buffers{values.data()};
  return Image(
    header,
    "sparse-binary",
    Image::ImageRepresentation::Image,
    Image::MultiComponentBufferType::SeparateImages,
    buffers);
}

Image makeThreeComponentImage()
{
  const glm::uvec3 dims{2, 2, 1};
  ImageIoInfo ioInfo = makeIoInfo(ComponentType::Float32, 3, dims);
  ImageHeader header(ioInfo, ioInfo, false);
  const std::vector<float> c0{1.0f, 2.0f, 3.0f, 4.0f};
  const std::vector<float> c1{4.0f, 3.0f, 2.0f, 1.0f};
  const std::vector<float> c2{2.0f, 6.0f, 10.0f, 14.0f};
  std::vector<const void*> buffers{c0.data(), c1.data(), c2.data()};
  return Image(
    header,
    "three-component",
    Image::ImageRepresentation::Image,
    Image::MultiComponentBufferType::SeparateImages,
    buffers);
}

Image makeTimeSeriesVectorImage()
{
  const glm::uvec3 dims{2, 2, 1};
  ImageIoInfo ioInfo = makeIoInfo(ComponentType::Float32, 3, dims);
  ImageHeader header(ioInfo, ioInfo, false);
  const std::vector<float> c0{1.0f, 2.0f, 3.0f, 4.0f, 11.0f, 12.0f, 13.0f, 14.0f};
  const std::vector<float> c1{5.0f, 6.0f, 7.0f, 8.0f, 15.0f, 16.0f, 17.0f, 18.0f};
  const std::vector<float> c2{9.0f, 10.0f, 11.0f, 12.0f, 19.0f, 20.0f, 21.0f, 22.0f};
  std::vector<const void*> buffers{c0.data(), c1.data(), c2.data()};
  return Image(
    header,
    "time-series-vector",
    Image::ImageRepresentation::Image,
    Image::MultiComponentBufferType::SeparateImages,
    buffers,
    ImageTimeAxis{2u, 0.0, 1.0, "sec"});
}

Image makeTimeSeriesLinearVectorField()
{
  const glm::uvec3 dims{3, 3, 3};
  ImageIoInfo ioInfo = makeIoInfo(ComponentType::Float32, 3, dims);
  ioInfo.m_spaceInfo.m_spacing = {2.0, 3.0, 4.0};
  ImageHeader header(ioInfo, ioInfo, false);

  std::vector<float> c0(2u * header.numPixels(), 0.0f);
  std::vector<float> c1(2u * header.numPixels(), 0.0f);
  std::vector<float> c2(2u * header.numPixels(), 0.0f);
  for (uint32_t timePoint = 0; timePoint < 2u; ++timePoint) {
    const float scale = static_cast<float>(timePoint + 1u);
    for (uint32_t z = 0; z < dims.z; ++z) {
      for (uint32_t y = 0; y < dims.y; ++y) {
        for (uint32_t x = 0; x < dims.x; ++x) {
          const std::size_t index = static_cast<std::size_t>(timePoint) * header.numPixels() +
                                    static_cast<std::size_t>(dims.x) * dims.y * z +
                                    static_cast<std::size_t>(dims.x) * y + x;
          c0[index] = scale * 0.5f * static_cast<float>(x);
          c1[index] = scale * (1.0f / 3.0f) * static_cast<float>(y);
          c2[index] = scale * 0.25f * static_cast<float>(z);
        }
      }
    }
  }

  std::vector<const void*> buffers{c0.data(), c1.data(), c2.data()};
  return Image(
    header,
    "time-series-linear-vector-field",
    Image::ImageRepresentation::Image,
    Image::MultiComponentBufferType::SeparateImages,
    buffers,
    ImageTimeAxis{2u, 0.0, 1.0, "sec"});
}

Image makeLinearVectorField(const std::vector<std::vector<double>>& directions = {})
{
  const glm::uvec3 dims{3, 3, 3};
  ImageIoInfo ioInfo = makeIoInfo(ComponentType::Float32, 3, dims);
  ioInfo.m_spaceInfo.m_spacing = {2.0, 3.0, 4.0};
  if (!directions.empty()) {
    ioInfo.m_spaceInfo.m_directions = directions;
  }
  ImageHeader header(ioInfo, ioInfo, false);

  std::vector<float> c0(header.numPixels(), 0.0f);
  std::vector<float> c1(header.numPixels(), 0.0f);
  std::vector<float> c2(header.numPixels(), 0.0f);
  for (uint32_t z = 0; z < dims.z; ++z) {
    for (uint32_t y = 0; y < dims.y; ++y) {
      for (uint32_t x = 0; x < dims.x; ++x) {
        const std::size_t index =
          static_cast<std::size_t>(dims.x) * dims.y * z + static_cast<std::size_t>(dims.x) * y + x;
        c0[index] = 0.5f * static_cast<float>(x);
        c1[index] = (1.0f / 3.0f) * static_cast<float>(y);
        c2[index] = 0.25f * static_cast<float>(z);
      }
    }
  }

  std::vector<const void*> buffers{c0.data(), c1.data(), c2.data()};
  return Image(
    header,
    "linear-vector-field",
    Image::ImageRepresentation::Image,
    Image::MultiComponentBufferType::SeparateImages,
    buffers);
}

Image makeQuadraticVectorField()
{
  const glm::uvec3 dims{3, 3, 3};
  ImageIoInfo ioInfo = makeIoInfo(ComponentType::Float32, 3, dims);
  ioInfo.m_spaceInfo.m_spacing = {2.0, 3.0, 4.0};
  ImageHeader header(ioInfo, ioInfo, false);

  std::vector<float> c0(header.numPixels(), 0.0f);
  std::vector<float> c1(header.numPixels(), 0.0f);
  std::vector<float> c2(header.numPixels(), 0.0f);
  for (uint32_t z = 0; z < dims.z; ++z) {
    for (uint32_t y = 0; y < dims.y; ++y) {
      for (uint32_t x = 0; x < dims.x; ++x) {
        const std::size_t index =
          static_cast<std::size_t>(dims.x) * dims.y * z + static_cast<std::size_t>(dims.x) * y + x;
        c0[index] = static_cast<float>(x * x);
        c1[index] = static_cast<float>(y * y);
        c2[index] = static_cast<float>(z * z);
      }
    }
  }

  std::vector<const void*> buffers{c0.data(), c1.data(), c2.data()};
  return Image(
    header,
    "quadratic-vector-field",
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

template<typename T>
ComponentType componentTypeFor();

template<>
ComponentType componentTypeFor<int8_t>()
{
  return ComponentType::Int8;
}

template<>
ComponentType componentTypeFor<uint8_t>()
{
  return ComponentType::UInt8;
}

template<>
ComponentType componentTypeFor<int16_t>()
{
  return ComponentType::Int16;
}

template<>
ComponentType componentTypeFor<uint16_t>()
{
  return ComponentType::UInt16;
}

template<>
ComponentType componentTypeFor<int32_t>()
{
  return ComponentType::Int32;
}

template<>
ComponentType componentTypeFor<uint32_t>()
{
  return ComponentType::UInt32;
}

template<>
ComponentType componentTypeFor<float>()
{
  return ComponentType::Float32;
}

template<typename T>
ComponentType sourceComponentTypeFor();

template<>
ComponentType sourceComponentTypeFor<uint8_t>()
{
  return ComponentType::UInt8;
}

template<>
ComponentType sourceComponentTypeFor<int8_t>()
{
  return ComponentType::Int8;
}

template<>
ComponentType sourceComponentTypeFor<uint16_t>()
{
  return ComponentType::UInt16;
}

template<>
ComponentType sourceComponentTypeFor<int16_t>()
{
  return ComponentType::Int16;
}

template<>
ComponentType sourceComponentTypeFor<uint32_t>()
{
  return ComponentType::UInt32;
}

template<>
ComponentType sourceComponentTypeFor<int32_t>()
{
  return ComponentType::Int32;
}

template<>
ComponentType sourceComponentTypeFor<unsigned long>()
{
  return ComponentType::ULong;
}

template<>
ComponentType sourceComponentTypeFor<long>()
{
  return ComponentType::Long;
}

template<>
ComponentType sourceComponentTypeFor<unsigned long long>()
{
  return ComponentType::ULongLong;
}

template<>
ComponentType sourceComponentTypeFor<long long>()
{
  return ComponentType::LongLong;
}

template<>
ComponentType sourceComponentTypeFor<double>()
{
  return ComponentType::Float64;
}

template<>
ComponentType sourceComponentTypeFor<long double>()
{
  return ComponentType::LongDouble;
}

template<typename T>
Image makeSeparateRawImage(std::vector<T>& values)
{
  const glm::uvec3 dims{static_cast<uint32_t>(values.size()), 1, 1};
  ImageIoInfo ioInfo = makeIoInfo(componentTypeFor<T>(), 1, dims);
  ImageHeader header(ioInfo, ioInfo, false);
  std::vector<const void*> buffers{values.data()};
  return Image(
    header,
    "raw-quantiles",
    Image::ImageRepresentation::Image,
    Image::MultiComponentBufferType::SeparateImages,
    buffers);
}

template<typename T>
void checkExactQuantilesForType()
{
  std::vector<T> values{static_cast<T>(4), static_cast<T>(2), static_cast<T>(2), static_cast<T>(8), static_cast<T>(10)};
  Image image = makeSeparateRawImage(values);

  REQUIRE(image.generateSortedBuffers());
  image.settings().setUsingExactQuantiles(true);

  CHECK(image.quantileToValue(0, 0.0) == Catch::Approx(2.0));
  CHECK(image.quantileToValue(0, 0.5) == Catch::Approx(4.0));
  CHECK(image.quantileToValue(0, 1.0) == Catch::Approx(10.0));

  const QuantileOfValue duplicate = image.valueToQuantile(0, int64_t{2});
  CHECK(duplicate.foundValue);
  CHECK(duplicate.lowerIndex == 0);
  CHECK(duplicate.upperIndex == 2);
  CHECK(duplicate.lowerValue == Catch::Approx(2.0));
  CHECK(duplicate.upperValue == Catch::Approx(4.0));
  CHECK(duplicate.lowerQuantile == Catch::Approx(0.0));
  CHECK(duplicate.upperQuantile == Catch::Approx(0.4));

  const QuantileOfValue middle = image.valueToQuantile(0, 8.0);
  CHECK(middle.foundValue);
  CHECK(middle.lowerValue == Catch::Approx(8.0));
  CHECK(middle.upperValue == Catch::Approx(10.0));

  const QuantileOfValue tooLow = image.valueToQuantile(0, int64_t{0});
  CHECK_FALSE(tooLow.foundValue);
  CHECK(tooLow.lowerIndex == 0);

  const QuantileOfValue tooHigh = image.valueToQuantile(0, 100.0);
  CHECK_FALSE(tooHigh.foundValue);
  CHECK(tooHigh.lowerIndex == values.size() - 1);
  CHECK(tooHigh.upperIndex == values.size() - 1);

  CHECK_THROWS_AS(image.quantileToValue(1, 0.5), std::exception);
  CHECK_THROWS_AS(image.valueToQuantile(1, int64_t{2}), std::exception);
  CHECK_THROWS_AS(image.valueToQuantile(1, 2.0), std::exception);
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

TEST_CASE("ITK enum mapping covers all supported pixel and component types", "[image][utility][itk]")
{
  CHECK(fromItkPixelType(itk::IOPixelEnum::UNKNOWNPIXELTYPE) == PixelType::Undefined);
  CHECK(fromItkPixelType(itk::IOPixelEnum::SCALAR) == PixelType::Scalar);
  CHECK(fromItkPixelType(itk::IOPixelEnum::RGB) == PixelType::RGB);
  CHECK(fromItkPixelType(itk::IOPixelEnum::RGBA) == PixelType::RGBA);
  CHECK(fromItkPixelType(itk::IOPixelEnum::OFFSET) == PixelType::Offset);
  CHECK(fromItkPixelType(itk::IOPixelEnum::VECTOR) == PixelType::Vector);
  CHECK(fromItkPixelType(itk::IOPixelEnum::POINT) == PixelType::Point);
  CHECK(fromItkPixelType(itk::IOPixelEnum::COVARIANTVECTOR) == PixelType::CovariantVector);
  CHECK(fromItkPixelType(itk::IOPixelEnum::SYMMETRICSECONDRANKTENSOR) == PixelType::SymmetricSecondRankTensor);
  CHECK(fromItkPixelType(itk::IOPixelEnum::DIFFUSIONTENSOR3D) == PixelType::DiffusionTensor3D);
  CHECK(fromItkPixelType(itk::IOPixelEnum::COMPLEX) == PixelType::Complex);
  CHECK(fromItkPixelType(itk::IOPixelEnum::FIXEDARRAY) == PixelType::FixedArray);
  CHECK(fromItkPixelType(itk::IOPixelEnum::ARRAY) == PixelType::Array);
  CHECK(fromItkPixelType(itk::IOPixelEnum::MATRIX) == PixelType::Matrix);
  CHECK(fromItkPixelType(itk::IOPixelEnum::VARIABLELENGTHVECTOR) == PixelType::VariableLengthVector);
  CHECK(fromItkPixelType(itk::IOPixelEnum::VARIABLESIZEMATRIX) == PixelType::VariableSizeMatrix);

  CHECK(fromItkComponentType(itk::IOComponentEnum::UCHAR) == ComponentType::UInt8);
  CHECK(fromItkComponentType(itk::IOComponentEnum::CHAR) == ComponentType::Int8);
  CHECK(fromItkComponentType(itk::IOComponentEnum::USHORT) == ComponentType::UInt16);
  CHECK(fromItkComponentType(itk::IOComponentEnum::SHORT) == ComponentType::Int16);
  CHECK(fromItkComponentType(itk::IOComponentEnum::UINT) == ComponentType::UInt32);
  CHECK(fromItkComponentType(itk::IOComponentEnum::INT) == ComponentType::Int32);
  CHECK(fromItkComponentType(itk::IOComponentEnum::FLOAT) == ComponentType::Float32);
  CHECK(fromItkComponentType(itk::IOComponentEnum::LONG) == ComponentType::Long);
  CHECK(fromItkComponentType(itk::IOComponentEnum::ULONG) == ComponentType::ULong);
  CHECK(fromItkComponentType(itk::IOComponentEnum::LONGLONG) == ComponentType::LongLong);
  CHECK(fromItkComponentType(itk::IOComponentEnum::ULONGLONG) == ComponentType::ULongLong);
  CHECK(fromItkComponentType(itk::IOComponentEnum::DOUBLE) == ComponentType::Float64);
  CHECK(fromItkComponentType(itk::IOComponentEnum::LDOUBLE) == ComponentType::LongDouble);
  CHECK(fromItkComponentType(itk::IOComponentEnum::UNKNOWNCOMPONENTTYPE) == ComponentType::Undefined);

  CHECK(toItkComponentType(ComponentType::Int8) == itk::IOComponentEnum::CHAR);
  CHECK(toItkComponentType(ComponentType::UInt8) == itk::IOComponentEnum::UCHAR);
  CHECK(toItkComponentType(ComponentType::Int16) == itk::IOComponentEnum::SHORT);
  CHECK(toItkComponentType(ComponentType::UInt16) == itk::IOComponentEnum::USHORT);
  CHECK(toItkComponentType(ComponentType::Int32) == itk::IOComponentEnum::INT);
  CHECK(toItkComponentType(ComponentType::UInt32) == itk::IOComponentEnum::UINT);
  CHECK(toItkComponentType(ComponentType::Float32) == itk::IOComponentEnum::FLOAT);
  CHECK(toItkComponentType(ComponentType::Float64) == itk::IOComponentEnum::DOUBLE);
  CHECK(toItkComponentType(ComponentType::Long) == itk::IOComponentEnum::LONG);
  CHECK(toItkComponentType(ComponentType::ULong) == itk::IOComponentEnum::ULONG);
  CHECK(toItkComponentType(ComponentType::LongLong) == itk::IOComponentEnum::LONGLONG);
  CHECK(toItkComponentType(ComponentType::ULongLong) == itk::IOComponentEnum::ULONG);
  CHECK(toItkComponentType(ComponentType::LongDouble) == itk::IOComponentEnum::LDOUBLE);
  CHECK(toItkComponentType(ComponentType::Undefined) == itk::IOComponentEnum::UNKNOWNCOMPONENTTYPE);
}

TEST_CASE("Raw component buffers convert and clamp every supported source type", "[image][buffer][cast]")
{
  auto checkSmallSignedIntegerSource = []<typename T>() {
    const std::vector<T> source{static_cast<T>(1), static_cast<T>(2), static_cast<T>(3)};
    const auto converted = createBuffer<T>(source.data(), source.size(), sourceComponentTypeFor<T>());
    REQUIRE(converted.size() == source.size());
    CHECK(converted == source);
  };

  auto checkSmallUnsignedIntegerSource = []<typename T>() {
    const std::vector<T> source{static_cast<T>(1), static_cast<T>(2), static_cast<T>(3)};
    const auto converted = createBuffer<T>(source.data(), source.size(), sourceComponentTypeFor<T>());
    REQUIRE(converted.size() == source.size());
    CHECK(converted == source);
  };

  checkSmallUnsignedIntegerSource.operator()<uint8_t>();
  checkSmallSignedIntegerSource.operator()<int8_t>();
  checkSmallUnsignedIntegerSource.operator()<uint16_t>();
  checkSmallSignedIntegerSource.operator()<int16_t>();
  checkSmallUnsignedIntegerSource.operator()<uint32_t>();
  checkSmallSignedIntegerSource.operator()<int32_t>();
  checkSmallUnsignedIntegerSource.operator()<unsigned long>();
  checkSmallSignedIntegerSource.operator()<long>();
  checkSmallUnsignedIntegerSource.operator()<unsigned long long>();
  checkSmallSignedIntegerSource.operator()<long long>();

  const std::vector<float> float32Source{1.25f, 2.5f, 3.75f};
  const auto float32Converted = createBuffer<float>(float32Source.data(), float32Source.size(), ComponentType::Float32);
  CHECK(float32Converted == float32Source);

  const std::vector<double> float64Source{1.25, 2.5, 3.75};
  const auto float64Converted = createBuffer<float>(float64Source.data(), float64Source.size(), ComponentType::Float64);
  REQUIRE(float64Converted.size() == float64Source.size());
  CHECK(float64Converted[0] == Catch::Approx(1.25f));
  CHECK(float64Converted[2] == Catch::Approx(3.75f));

  const std::vector<long double> longDoubleSource{
    static_cast<long double>(-1.5),
    static_cast<long double>(0.5),
    static_cast<long double>(2.5)};
  const auto longDoubleConverted =
    createBuffer<float>(longDoubleSource.data(), longDoubleSource.size(), ComponentType::LongDouble);
  REQUIRE(longDoubleConverted.size() == longDoubleSource.size());
  CHECK(longDoubleConverted[0] == Catch::Approx(-1.5f));
  CHECK(longDoubleConverted[2] == Catch::Approx(2.5f));

  const std::vector<int16_t> signedSource{-200, 0, 200};
  const auto clampedToInt8 = createBuffer<int8_t>(signedSource.data(), signedSource.size(), ComponentType::Int16);
  REQUIRE(clampedToInt8.size() == signedSource.size());
  CHECK(clampedToInt8[0] == std::numeric_limits<int8_t>::lowest());
  CHECK(clampedToInt8[1] == 0);
  CHECK(clampedToInt8[2] == std::numeric_limits<int8_t>::max());

  const auto nullConverted = createBuffer<uint16_t>(nullptr, 3, ComponentType::UInt16);
  CHECK(nullConverted == std::vector<uint16_t>{0, 0, 0});
  CHECK_THROWS_AS(
    createBuffer<uint8_t>(signedSource.data(), signedSource.size(), ComponentType::Undefined),
    std::exception);
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

TEST_CASE("Image display names strip known medical image extensions case-insensitively", "[image][utility]")
{
  CHECK(getFileName("/tmp/brain.nii.gz", false) == "brain");
  CHECK(getFileName("/tmp/brain.NII.GZ", false) == "brain");
  CHECK(getFileName("/tmp/brain.nii", false) == "brain");
  CHECK(getFileName("/tmp/brain.NII", false) == "brain");
  CHECK(getFileName("/tmp/atlas.mhd.gz", false) == "atlas");
  CHECK(getFileName("/tmp/atlas.MHD.GZ", false) == "atlas");
  CHECK(getFileName("/tmp/atlas.mhd", false) == "atlas");
  CHECK(getFileName("/tmp/segmentation.nrrd", false) == "segmentation");
  CHECK(getFileName("/tmp/analyze.hdr", false) == "analyze");
  CHECK(getFileName("/tmp/analyze.img", false) == "analyze");

  CHECK(getFileName("/tmp/report.txt", false) == "report.txt");
  CHECK(getFileName("/tmp/archive.tar.gz", false) == "archive.tar.gz");
  CHECK(getFileName("/tmp/brain.nii.gz", true) == "brain.nii.gz");
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

TEST_CASE("Raw sparse binary images initialize full-range window and level", "[image][raw][settings]")
{
  Image image = makeSparseBinaryImage();

  CHECK(image.settings().componentStatistics(0).onlineStats.min == Catch::Approx(0.0));
  CHECK(image.settings().componentStatistics(0).onlineStats.max == Catch::Approx(100.0));
  CHECK(image.settings().windowWidth(0) == Catch::Approx(100.0));
  CHECK(image.settings().windowCenter(0) == Catch::Approx(50.0));
  CHECK(image.settings().windowValuesLowHigh(0) == std::pair<double, double>{0.0, 100.0});
}

TEST_CASE("Raw time-series vector images sample the requested time frame", "[image][raw][time][vector]")
{
  Image image = makeTimeSeriesVectorImage();

  CHECK(image.isTimeSeries());
  CHECK(image.value<double>(0, 0, 0).value() == Catch::Approx(1.0));
  CHECK(image.value<double>(0, 0, 1).value() == Catch::Approx(11.0));
  CHECK(image.valueLinear<double>(0, 0.5, 0.0, 0.0, 0).value() == Catch::Approx(1.5));
  CHECK(image.valueLinear<double>(0, 0.5, 0.0, 0.0, 1).value() == Catch::Approx(11.5));
  CHECK(image.valueLinear<double>(2, 1.0, 1.0, 0.0, 1).value() == Catch::Approx(22.0));
}

TEST_CASE("Component projections use the requested source time frame", "[image][derived][time][vector]")
{
  Image image = makeTimeSeriesVectorImage();

  const auto magnitude0 = createComponentProjectionImage(image, ComponentProjectionMode::Magnitude, 0);
  const auto magnitude1 = createComponentProjectionImage(image, ComponentProjectionMode::Magnitude, 1);
  REQUIRE(magnitude0);
  REQUIRE(magnitude1);

  CHECK(magnitude0->value<double>(0, 0).value() == Catch::Approx(std::sqrt(1.0 * 1.0 + 5.0 * 5.0 + 9.0 * 9.0)));
  CHECK(magnitude1->value<double>(0, 0).value() == Catch::Approx(std::sqrt(11.0 * 11.0 + 15.0 * 15.0 + 19.0 * 19.0)));
}

TEST_CASE("Vector derivative projections use the requested source time frame", "[image][derived][time][vector]")
{
  Image image = makeTimeSeriesLinearVectorField();
  const glm::uvec3 centerVoxel{1u, 1u, 1u};

  const double expectedJacobian0 = (1.0 + 0.25) * (1.0 + 1.0 / 9.0) * (1.0 + 0.0625);
  const double expectedJacobian1 = (1.0 + 0.5) * (1.0 + 2.0 / 9.0) * (1.0 + 0.125);

  CHECK(
    vectorDerivativeProjectionValue(image, ComponentProjectionMode::VectorJacobianDeterminant, centerVoxel, 0)
      .value() == Catch::Approx(expectedJacobian0));
  CHECK(
    vectorDerivativeProjectionValue(image, ComponentProjectionMode::VectorJacobianDeterminant, centerVoxel, 1)
      .value() == Catch::Approx(expectedJacobian1));

  const auto jacobian0 = createComponentProjectionImage(image, ComponentProjectionMode::VectorJacobianDeterminant, 0);
  const auto jacobian1 = createComponentProjectionImage(image, ComponentProjectionMode::VectorJacobianDeterminant, 1);
  REQUIRE(jacobian0);
  REQUIRE(jacobian1);

  CHECK(jacobian0->value<double>(0, 13).value() == Catch::Approx(expectedJacobian0));
  CHECK(jacobian1->value<double>(0, 13).value() == Catch::Approx(expectedJacobian1));
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

TEST_CASE("Component projections create scalar images from multi-component images", "[image][derived]")
{
  const Image image = makeThreeComponentImage();

  const auto minimum = createComponentProjectionImage(image, ComponentProjectionMode::Minimum);
  REQUIRE(minimum.has_value());
  CHECK(minimum->header().memoryComponentType() == ComponentType::Float32);
  CHECK(minimum->header().numComponentsPerPixel() == 1);
  CHECK(minimum->value<double>(0, 0).value() == Catch::Approx(1.0));
  CHECK(minimum->value<double>(0, 1).value() == Catch::Approx(2.0));
  CHECK(minimum->value<double>(0, 2).value() == Catch::Approx(2.0));
  CHECK(minimum->value<double>(0, 3).value() == Catch::Approx(1.0));

  const auto mean = createComponentProjectionImage(image, ComponentProjectionMode::Mean);
  REQUIRE(mean.has_value());
  CHECK(mean->value<double>(0, 0).value() == Catch::Approx(7.0 / 3.0));
  CHECK(mean->value<double>(0, 1).value() == Catch::Approx(11.0 / 3.0));
  CHECK(mean->value<double>(0, 2).value() == Catch::Approx(5.0));
  CHECK(mean->value<double>(0, 3).value() == Catch::Approx(19.0 / 3.0));

  const auto maximum = createComponentProjectionImage(image, ComponentProjectionMode::Maximum);
  REQUIRE(maximum.has_value());
  CHECK(maximum->value<double>(0, 0).value() == Catch::Approx(4.0));
  CHECK(maximum->value<double>(0, 1).value() == Catch::Approx(6.0));
  CHECK(maximum->value<double>(0, 2).value() == Catch::Approx(10.0));
  CHECK(maximum->value<double>(0, 3).value() == Catch::Approx(14.0));

  const auto magnitude = createComponentProjectionImage(image, ComponentProjectionMode::Magnitude);
  REQUIRE(magnitude.has_value());
  CHECK(magnitude->value<double>(0, 0).value() == Catch::Approx(std::sqrt(21.0)));
  CHECK(magnitude->value<double>(0, 1).value() == Catch::Approx(7.0));
  CHECK(magnitude->value<double>(0, 2).value() == Catch::Approx(std::sqrt(113.0)));
  CHECK(magnitude->value<double>(0, 3).value() == Catch::Approx(std::sqrt(213.0)));
}

TEST_CASE("Component render modes map to scalar projection modes", "[image][derived]")
{
  CHECK_FALSE(componentProjectionFromRenderMode(ComponentRenderMode::SingleComponent).has_value());
  CHECK_FALSE(componentProjectionFromRenderMode(ComponentRenderMode::Color).has_value());
  CHECK_FALSE(componentProjectionFromRenderMode(ComponentRenderMode::ComplexReal).has_value());
  CHECK_FALSE(componentProjectionFromRenderMode(ComponentRenderMode::ComplexImaginary).has_value());
  CHECK_FALSE(componentProjectionFromRenderMode(ComponentRenderMode::VectorDirectionColor).has_value());
  CHECK_FALSE(componentProjectionFromRenderMode(ComponentRenderMode::VectorSignedNormalProjection).has_value());
  CHECK_FALSE(componentProjectionFromRenderMode(ComponentRenderMode::VectorPlanarProjectionColor).has_value());
  CHECK(componentProjectionFromRenderMode(ComponentRenderMode::Minimum) == ComponentProjectionMode::Minimum);
  CHECK(componentProjectionFromRenderMode(ComponentRenderMode::Mean) == ComponentProjectionMode::Mean);
  CHECK(componentProjectionFromRenderMode(ComponentRenderMode::Maximum) == ComponentProjectionMode::Maximum);
  CHECK(componentProjectionFromRenderMode(ComponentRenderMode::Magnitude) == ComponentProjectionMode::Magnitude);
  CHECK(
    componentProjectionFromRenderMode(ComponentRenderMode::ComplexPhase) ==
    ComponentProjectionMode::ComplexPhaseSignedRadians);
  CHECK(
    componentProjectionFromRenderMode(ComponentRenderMode::VectorGradientMagnitude) ==
    ComponentProjectionMode::VectorGradientMagnitude);
  CHECK(
    componentProjectionFromRenderMode(ComponentRenderMode::VectorLaplacianMagnitude) ==
    ComponentProjectionMode::VectorLaplacianMagnitude);
}

TEST_CASE("Complex phase helpers support signed and unsigned radians and degrees", "[image][derived][complex]")
{
  CHECK(
    complexPhaseValue(0.0, 1.0, ComplexPhaseRange::Signed, ComplexPhaseUnit::Radians) ==
    Catch::Approx(std::numbers::pi / 2.0));
  CHECK(
    complexPhaseValue(0.0, -1.0, ComplexPhaseRange::Unsigned, ComplexPhaseUnit::Radians) ==
    Catch::Approx(3.0 * std::numbers::pi / 2.0));
  CHECK(complexPhaseValue(-1.0, 0.0, ComplexPhaseRange::Signed, ComplexPhaseUnit::Degrees) == Catch::Approx(180.0));
  CHECK(complexPhaseValue(0.0, -1.0, ComplexPhaseRange::Unsigned, ComplexPhaseUnit::Degrees) == Catch::Approx(270.0));
  CHECK(complexComponentLabel(0, 1) == "0 of 1 (real)");
  CHECK(complexComponentLabel(1, 1) == "1 of 1 (imaginary)");
}

TEST_CASE("Complex phase projection creates scalar images from complex components", "[image][derived][complex]")
{
  const glm::uvec3 dims{2, 2, 1};
  ImageIoInfo ioInfo = makeIoInfo(ComponentType::Float32, 2, dims);
  ioInfo.m_pixelInfo.m_pixelType = PixelType::Complex;
  ioInfo.m_pixelInfo.m_pixelTypeString = "complex";
  ImageHeader header(ioInfo, ioInfo, false);

  const std::vector<float> real{1.0f, 0.0f, 0.0f, -1.0f};
  const std::vector<float> imaginary{0.0f, 1.0f, -1.0f, 0.0f};
  std::vector<const void*> buffers{real.data(), imaginary.data()};
  Image image(
    header,
    "complex",
    Image::ImageRepresentation::Image,
    Image::MultiComponentBufferType::SeparateImages,
    buffers);

  CHECK(isComplexValuedImage(image));
  CHECK(image.settings().componentRenderMode() == ComponentRenderMode::Magnitude);

  image.settings().setComponentRenderMode(ComponentRenderMode::ComplexPhase);
  image.settings().setComplexPhaseRange(ComplexPhaseRange::Unsigned);
  image.settings().setComplexPhaseUnit(ComplexPhaseUnit::Degrees);
  CHECK(componentProjectionForImage(image) == ComponentProjectionMode::ComplexPhaseUnsignedDegrees);

  const auto phase = createComponentProjectionImage(image, *componentProjectionForImage(image));
  REQUIRE(phase.has_value());
  CHECK(phase->header().numComponentsPerPixel() == 1);
  CHECK(phase->value<double>(0, 0).value() == Catch::Approx(0.0));
  CHECK(phase->value<double>(0, 1).value() == Catch::Approx(90.0));
  CHECK(phase->value<double>(0, 2).value() == Catch::Approx(270.0));
  CHECK(phase->value<double>(0, 3).value() == Catch::Approx(180.0));
}

TEST_CASE("Component projections ignore non-finite component values", "[image][derived]")
{
  const glm::uvec3 dims{2, 1, 1};
  ImageIoInfo ioInfo = makeIoInfo(ComponentType::Float32, 3, dims);
  ImageHeader header(ioInfo, ioInfo, false);

  const float nan = std::numeric_limits<float>::quiet_NaN();
  const std::vector<float> c0{nan, nan};
  const std::vector<float> c1{3.0f, nan};
  const std::vector<float> c2{4.0f, nan};
  std::vector<const void*> buffers{c0.data(), c1.data(), c2.data()};
  const Image image(
    header,
    "non-finite-components",
    Image::ImageRepresentation::Image,
    Image::MultiComponentBufferType::SeparateImages,
    buffers);

  const auto minimum = createComponentProjectionImage(image, ComponentProjectionMode::Minimum);
  REQUIRE(minimum.has_value());
  CHECK(minimum->value<double>(0, 0).value() == Catch::Approx(3.0));
  CHECK(minimum->value<double>(0, 1).value() == Catch::Approx(0.0));

  const auto mean = createComponentProjectionImage(image, ComponentProjectionMode::Mean);
  REQUIRE(mean.has_value());
  CHECK(mean->value<double>(0, 0).value() == Catch::Approx(3.5));
  CHECK(mean->value<double>(0, 1).value() == Catch::Approx(0.0));

  const auto maximum = createComponentProjectionImage(image, ComponentProjectionMode::Maximum);
  REQUIRE(maximum.has_value());
  CHECK(maximum->value<double>(0, 0).value() == Catch::Approx(4.0));
  CHECK(maximum->value<double>(0, 1).value() == Catch::Approx(0.0));

  const auto magnitude = createComponentProjectionImage(image, ComponentProjectionMode::Magnitude);
  REQUIRE(magnitude.has_value());
  CHECK(magnitude->value<double>(0, 0).value() == Catch::Approx(5.0));
  CHECK(magnitude->value<double>(0, 1).value() == Catch::Approx(0.0));
}

TEST_CASE("Component projections read logical components from interleaved images", "[image][derived]")
{
  const glm::uvec3 dims{2, 1, 1};
  ImageIoInfo ioInfo = makeIoInfo(ComponentType::Float32, 3, dims);
  ImageHeader header(ioInfo, ioInfo, true);
  const std::vector<float> interleaved{1.0f, 4.0f, 2.0f, 2.0f, 3.0f, 6.0f};
  std::vector<const void*> buffers{interleaved.data()};
  const Image image(
    header,
    "interleaved-components",
    Image::ImageRepresentation::Image,
    Image::MultiComponentBufferType::InterleavedImage,
    buffers);

  const auto mean = createComponentProjectionImage(image, ComponentProjectionMode::Mean);
  REQUIRE(mean.has_value());
  CHECK(mean->value<double>(0, 0).value() == Catch::Approx(7.0 / 3.0));
  CHECK(mean->value<double>(0, 1).value() == Catch::Approx(11.0 / 3.0));

  const auto magnitude = createComponentProjectionImage(image, ComponentProjectionMode::Magnitude);
  REQUIRE(magnitude.has_value());
  CHECK(magnitude->value<double>(0, 0).value() == Catch::Approx(std::sqrt(21.0)));
  CHECK(magnitude->value<double>(0, 1).value() == Catch::Approx(7.0));
}

TEST_CASE("Vector field derivative projections use physical spacing", "[image][derived][vector]")
{
  Image image = makeLinearVectorField();

  CHECK(isVectorFieldCandidate(image));
  CHECK(isVectorFieldImage(image));
  CHECK(image.settings().vectorWarpedGridVoxelSpacing() == Catch::Approx(4.0f));
  CHECK(image.settings().vectorWarpedGridMillimeterSpacing() == Catch::Approx(8.0f));
  CHECK(vectorFieldComponentLabel(0, 2) == "0 of 2 (x)");
  CHECK(vectorFieldComponentLabel(1, 2) == "1 of 2 (y)");
  CHECK(vectorFieldComponentLabel(2, 2) == "2 of 2 (z)");

  const glm::uvec3 centerVoxel{1, 1, 1};
  const auto jacobian = createComponentProjectionImage(image, ComponentProjectionMode::VectorJacobianDeterminant);
  REQUIRE(jacobian.has_value());
  const double expectedJacobian = (1.0 + 0.25) * (1.0 + 1.0 / 9.0) * (1.0 + 0.0625);
  CHECK(jacobian->value<double>(0, 13).value() == Catch::Approx(expectedJacobian));
  CHECK(
    vectorDerivativeProjectionValue(image, ComponentProjectionMode::VectorJacobianDeterminant, centerVoxel).value() ==
    Catch::Approx(expectedJacobian));

  image.settings().setComponentRenderMode(ComponentRenderMode::VectorJacobianDeterminant);
  CHECK(componentProjectionForImage(image) == ComponentProjectionMode::VectorJacobianDeterminant);
  image.settings().setVectorLogJacobianDeterminant(true);
  CHECK(componentProjectionForImage(image) == ComponentProjectionMode::VectorLogJacobianDeterminant);
  const auto logJacobian = createComponentProjectionImage(image, ComponentProjectionMode::VectorLogJacobianDeterminant);
  REQUIRE(logJacobian.has_value());
  CHECK(logJacobian->value<double>(0, 13).value() == Catch::Approx(std::log(expectedJacobian)));

  const auto gradient = createComponentProjectionImage(image, ComponentProjectionMode::VectorGradientMagnitude);
  REQUIRE(gradient.has_value());
  CHECK(
    gradient->value<double>(0, 13).value() ==
    Catch::Approx(std::sqrt(0.25 * 0.25 + (1.0 / 9.0) * (1.0 / 9.0) + 0.0625 * 0.0625)));
  CHECK(
    vectorDerivativeProjectionValue(image, ComponentProjectionMode::VectorGradientMagnitude, centerVoxel).value() ==
    Catch::Approx(std::sqrt(0.25 * 0.25 + (1.0 / 9.0) * (1.0 / 9.0) + 0.0625 * 0.0625)));

  const auto divergence = createComponentProjectionImage(image, ComponentProjectionMode::VectorDivergence);
  REQUIRE(divergence.has_value());
  CHECK(divergence->value<double>(0, 13).value() == Catch::Approx(0.25 + 1.0 / 9.0 + 0.0625));
  CHECK(
    vectorDerivativeProjectionValue(image, ComponentProjectionMode::VectorDivergence, centerVoxel).value() ==
    Catch::Approx(0.25 + 1.0 / 9.0 + 0.0625));

  const auto curl = createComponentProjectionImage(image, ComponentProjectionMode::VectorCurlMagnitude);
  REQUIRE(curl.has_value());
  CHECK(curl->value<double>(0, 13).value() == Catch::Approx(0.0));
  CHECK(
    vectorDerivativeProjectionValue(image, ComponentProjectionMode::VectorCurlMagnitude, centerVoxel).value() ==
    Catch::Approx(0.0));
  const auto laplacian = createComponentProjectionImage(image, ComponentProjectionMode::VectorLaplacianMagnitude);
  REQUIRE(laplacian.has_value());
  CHECK(laplacian->value<double>(0, 13).value() == Catch::Approx(0.0));
  CHECK(
    vectorDerivativeProjectionValue(image, ComponentProjectionMode::VectorLaplacianMagnitude, centerVoxel).value() ==
    Catch::Approx(0.0));
  CHECK_FALSE(vectorDerivativeProjectionValue(image, ComponentProjectionMode::Magnitude, centerVoxel).has_value());
  CHECK_FALSE(
    vectorDerivativeProjectionValue(image, ComponentProjectionMode::VectorDivergence, glm::uvec3{3, 1, 1}).has_value());
}

TEST_CASE("Vector field derivative projections respect image directions", "[image][derived][vector]")
{
  Image image = makeLinearVectorField({{0.0, 1.0, 0.0}, {-1.0, 0.0, 0.0}, {0.0, 0.0, 1.0}});
  const glm::uvec3 centerVoxel{1, 1, 1};

  CHECK(
    vectorDerivativeProjectionValue(image, ComponentProjectionMode::VectorDivergence, centerVoxel).value() ==
    Catch::Approx(0.0625));
  CHECK(
    vectorDerivativeProjectionValue(image, ComponentProjectionMode::VectorCurlMagnitude, centerVoxel).value() ==
    Catch::Approx(0.25 + 1.0 / 9.0));

  const double expectedJacobian = (1.0 + 0.0625) * (1.0 + 0.25 / 9.0);
  CHECK(
    vectorDerivativeProjectionValue(image, ComponentProjectionMode::VectorJacobianDeterminant, centerVoxel).value() ==
    Catch::Approx(expectedJacobian));
}

TEST_CASE("Vector field Laplacian magnitude handles nonlinear fields", "[image][derived][vector]")
{
  const Image image = makeQuadraticVectorField();
  const glm::uvec3 centerVoxel{1, 1, 1};

  const double expectedLaplacianMagnitude = std::sqrt(0.5 * 0.5 + (2.0 / 9.0) * (2.0 / 9.0) + 0.125 * 0.125);
  CHECK(
    vectorDerivativeProjectionValue(image, ComponentProjectionMode::VectorLaplacianMagnitude, centerVoxel).value() ==
    Catch::Approx(expectedLaplacianMagnitude));

  const auto laplacian = createComponentProjectionImage(image, ComponentProjectionMode::VectorLaplacianMagnitude);
  REQUIRE(laplacian.has_value());
  CHECK(laplacian->value<double>(0, 13).value() == Catch::Approx(expectedLaplacianMagnitude));
}

TEST_CASE("Exact image quantiles work for every in-memory component type", "[image][quantiles]")
{
  checkExactQuantilesForType<int8_t>();
  checkExactQuantilesForType<uint8_t>();
  checkExactQuantilesForType<int16_t>();
  checkExactQuantilesForType<uint16_t>();
  checkExactQuantilesForType<int32_t>();
  checkExactQuantilesForType<uint32_t>();
  checkExactQuantilesForType<float>();
}

TEST_CASE("Approximate image quantiles use T-digest data before exact buffers are enabled", "[image][quantiles]")
{
  std::vector<uint16_t> values{0, 10, 20, 30, 40, 50};
  Image image = makeSeparateRawImage(values);

  REQUIRE_FALSE(image.settings().usingExactQuantiles());

  CHECK(image.quantileToValue(0, 0.0) == Catch::Approx(0.0));
  CHECK(image.quantileToValue(0, 1.0) == Catch::Approx(50.0));

  const QuantileOfValue q = image.valueToQuantile(0, int64_t{30});
  CHECK(q.foundValue);
  CHECK(q.lowerValue == Catch::Approx(30.0));
  CHECK(q.upperValue == Catch::Approx(30.0));
  CHECK(q.lowerQuantile == Catch::Approx(q.upperQuantile));
  CHECK(q.lowerQuantile > 0.0);
  CHECK(q.lowerQuantile < 1.0);

  const QuantileOfValue qDouble = image.valueToQuantile(0, 30.0);
  CHECK(qDouble.foundValue);
  CHECK(qDouble.lowerValue == Catch::Approx(30.0));

  CHECK_THROWS_AS(image.quantileToValue(1, 0.5), std::exception);
  CHECK_THROWS_AS(image.valueToQuantile(1, int64_t{30}), std::exception);
  CHECK_THROWS_AS(image.valueToQuantile(1, 30.0), std::exception);
}

TEST_CASE("Exact quantiles split interleaved component buffers before sorting", "[image][quantiles][interleaved]")
{
  const glm::uvec3 dims{3, 1, 1};
  ImageIoInfo ioInfo = makeIoInfo(ComponentType::Int16, 3, dims);
  ImageHeader header(ioInfo, ioInfo, true);

  const std::vector<int16_t> interleavedValues{30, 1, 300, 10, 3, 100, 20, 2, 200};
  std::vector<const void*> buffers{interleavedValues.data()};

  Image image(
    header,
    "raw-interleaved-quantiles",
    Image::ImageRepresentation::Image,
    Image::MultiComponentBufferType::InterleavedImage,
    buffers);

  REQUIRE(image.generateSortedBuffers());
  image.settings().setUsingExactQuantiles(true);

  CHECK(image.quantileToValue(0, 0.0) == Catch::Approx(10.0));
  CHECK(image.quantileToValue(0, 1.0) == Catch::Approx(30.0));
  CHECK(image.quantileToValue(1, 0.5) == Catch::Approx(2.0));
  CHECK(image.quantileToValue(2, 1.0) == Catch::Approx(300.0));
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
