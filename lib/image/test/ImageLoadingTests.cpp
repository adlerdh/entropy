#include "image/Image.h"
#include "image/ImageUtility.h"
#include "image/internal/ImageUtilityItk.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <itkImage.h>
#include <itkImageFileWriter.h>
#include <itkMetaDataObject.h>
#include <itkRGBPixel.h>
#include <itkVectorImage.h>

#include <array>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <string>
#include <type_traits>
#include <variant>
#include <vector>

namespace
{
namespace fs = std::filesystem;

using BufferType = Image::MultiComponentBufferType;
using Rep = Image::ImageRepresentation;

fs::path testDirectory()
{
  const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
  fs::path dir = fs::temp_directory_path() / ("entropy-image-tests-" + std::to_string(stamp));
  fs::create_directories(dir);
  return dir;
}

template<unsigned int NDim>
typename itk::ImageBase<NDim>::DirectionType identityDirection()
{
  typename itk::ImageBase<NDim>::DirectionType direction;
  direction.SetIdentity();
  return direction;
}

template<typename T, unsigned int NDim>
fs::path writeScalarImage(
  const fs::path& dir,
  const std::string& name,
  const std::array<std::size_t, NDim>& dims,
  const std::array<double, NDim>& spacing,
  const std::array<double, NDim>& origin,
  const std::vector<T>& values,
  const std::string& extension = ".nrrd")
{
  using ImageType = itk::Image<T, NDim>;
  using WriterType = itk::ImageFileWriter<ImageType>;

  typename ImageType::IndexType start;
  typename ImageType::SizeType size;
  typename ImageType::PointType itkOrigin{};
  typename ImageType::SpacingType itkSpacing{};
  start.Fill(0);

  std::size_t numPixels = 1;
  for (unsigned int i = 0; i < NDim; ++i) {
    size[i] = dims[i];
    itkOrigin[i] = origin[i];
    itkSpacing[i] = spacing[i];
    numPixels *= dims[i];
  }
  REQUIRE(values.size() == numPixels);

  typename ImageType::RegionType region;
  region.SetIndex(start);
  region.SetSize(size);

  typename ImageType::Pointer image = ImageType::New();
  image->SetRegions(region);
  image->SetOrigin(itkOrigin);
  image->SetSpacing(itkSpacing);
  image->SetDirection(identityDirection<NDim>());
  image->Allocate();
  std::copy(values.begin(), values.end(), image->GetBufferPointer());

  const fs::path fileName = dir / (name + extension);
  typename WriterType::Pointer writer = WriterType::New();
  writer->SetFileName(fileName.string());
  writer->SetInput(image);
  writer->UseCompressionOff();
  writer->Update();
  return fileName;
}

template<typename T>
fs::path writeVectorImage(
  const fs::path& dir,
  const std::string& name,
  const std::array<std::size_t, 3>& dims,
  const std::array<double, 3>& spacing,
  const std::array<double, 3>& origin,
  unsigned int numComponents,
  const std::vector<T>& interleavedValues)
{
  using ImageType = itk::VectorImage<T, 3>;
  using WriterType = itk::ImageFileWriter<ImageType>;

  typename ImageType::IndexType start;
  typename ImageType::SizeType size;
  typename ImageType::PointType itkOrigin{};
  typename ImageType::SpacingType itkSpacing{};
  start.Fill(0);

  std::size_t numPixels = 1;
  for (unsigned int i = 0; i < 3; ++i) {
    size[i] = dims[i];
    itkOrigin[i] = origin[i];
    itkSpacing[i] = spacing[i];
    numPixels *= dims[i];
  }
  REQUIRE(interleavedValues.size() == numPixels * numComponents);

  typename ImageType::RegionType region;
  region.SetIndex(start);
  region.SetSize(size);

  typename ImageType::Pointer image = ImageType::New();
  image->SetVectorLength(numComponents);
  image->SetRegions(region);
  image->SetOrigin(itkOrigin);
  image->SetSpacing(itkSpacing);
  image->SetDirection(identityDirection<3>());
  image->Allocate();

  std::copy(interleavedValues.begin(), interleavedValues.end(), image->GetBufferPointer());

  const fs::path fileName = dir / (name + ".nrrd");
  typename WriterType::Pointer writer = WriterType::New();
  writer->SetFileName(fileName.string());
  writer->SetInput(image);
  writer->UseCompressionOff();
  writer->Update();
  return fileName;
}

template<typename T, unsigned int NDim>
fs::path writeVectorImage(
  const fs::path& dir,
  const std::string& name,
  const std::array<std::size_t, NDim>& dims,
  const std::array<double, NDim>& spacing,
  const std::array<double, NDim>& origin,
  unsigned int numComponents,
  const std::vector<T>& interleavedValues,
  const std::string& extension = ".nrrd")
{
  using ImageType = itk::VectorImage<T, NDim>;
  using WriterType = itk::ImageFileWriter<ImageType>;

  typename ImageType::IndexType start;
  typename ImageType::SizeType size;
  typename ImageType::PointType itkOrigin{};
  typename ImageType::SpacingType itkSpacing{};
  start.Fill(0);

  std::size_t numPixels = 1;
  for (unsigned int i = 0; i < NDim; ++i) {
    size[i] = dims[i];
    itkOrigin[i] = origin[i];
    itkSpacing[i] = spacing[i];
    numPixels *= dims[i];
  }
  REQUIRE(interleavedValues.size() == numPixels * numComponents);

  typename ImageType::RegionType region;
  region.SetIndex(start);
  region.SetSize(size);

  typename ImageType::Pointer image = ImageType::New();
  image->SetVectorLength(numComponents);
  image->SetRegions(region);
  image->SetOrigin(itkOrigin);
  image->SetSpacing(itkSpacing);
  image->SetDirection(identityDirection<NDim>());
  image->Allocate();

  std::copy(interleavedValues.begin(), interleavedValues.end(), image->GetBufferPointer());

  const fs::path fileName = dir / (name + extension);
  typename WriterType::Pointer writer = WriterType::New();
  writer->SetFileName(fileName.string());
  writer->SetInput(image);
  writer->UseCompressionOff();
  writer->Update();
  return fileName;
}

fs::path writeRgbImage(const fs::path& dir)
{
  using PixelType = itk::RGBPixel<uint8_t>;
  using ImageType = itk::Image<PixelType, 3>;
  using WriterType = itk::ImageFileWriter<ImageType>;

  ImageType::IndexType start;
  ImageType::SizeType size;
  start.Fill(0);
  size[0] = 2;
  size[1] = 1;
  size[2] = 1;

  ImageType::RegionType region;
  region.SetIndex(start);
  region.SetSize(size);

  ImageType::Pointer image = ImageType::New();
  image->SetRegions(region);
  image->SetDirection(identityDirection<3>());
  image->Allocate();

  PixelType first;
  first.SetRed(1);
  first.SetGreen(2);
  first.SetBlue(3);
  PixelType second;
  second.SetRed(4);
  second.SetGreen(5);
  second.SetBlue(6);
  image->GetBufferPointer()[0] = first;
  image->GetBufferPointer()[1] = second;

  const fs::path fileName = dir / "rgb.nrrd";
  WriterType::Pointer writer = WriterType::New();
  writer->SetFileName(fileName.string());
  writer->SetInput(image);
  writer->UseCompressionOff();
  writer->Update();
  return fileName;
}

fs::path writeNrrdVectorAxisImage(const fs::path& dir)
{
  const fs::path fileName = dir / "nrrd-vector-axis.nrrd";
  std::ofstream out(fileName, std::ios::binary);
  out << "NRRD0004\n"
      << "type: float\n"
      << "dimension: 4\n"
      << "space: left-posterior-superior\n"
      << "sizes: 5 2 2 1\n"
      << "space directions: none (1,0,0) (0,1,0) (0,0,1)\n"
      << "kinds: vector domain domain domain\n"
      << "endian: little\n"
      << "encoding: raw\n"
      << "space origin: (0,0,0)\n"
      << "\n";

  const std::array<float, 20> values{0.0f,  1.0f,  2.0f,  3.0f,  4.0f,  10.0f, 11.0f, 12.0f, 13.0f, 14.0f,
                                     20.0f, 21.0f, 22.0f, 23.0f, 24.0f, 30.0f, 31.0f, 32.0f, 33.0f, 34.0f};
  out.write(reinterpret_cast<const char*>(values.data()), static_cast<std::streamsize>(values.size() * sizeof(float)));
  return fileName;
}

fs::path writeNrrdVectorAxisTimeImage(const fs::path& dir)
{
  const fs::path fileName = dir / "nrrd-vector-axis-time.nrrd";
  std::ofstream out(fileName, std::ios::binary);
  out << "NRRD0004\n"
      << "type: float\n"
      << "dimension: 5\n"
      << "space dimension: 4\n"
      << "sizes: 2 2 1 1 3\n"
      << "space directions: none (1,0,0,0) (0,1,0,0) (0,0,1,0) (0,0,0,2)\n"
      << "kinds: vector domain domain domain domain\n"
      << "endian: little\n"
      << "encoding: raw\n"
      << "space origin: (0,0,0,10)\n"
      << "entropy_time_axis:=last\n"
      << "entropy_time_units:=frame\n"
      << "\n";

  const std::array<float, 12>
    values{0.0f, 1.0f, 10.0f, 11.0f, 100.0f, 101.0f, 110.0f, 111.0f, 200.0f, 201.0f, 210.0f, 211.0f};
  out.write(reinterpret_cast<const char*>(values.data()), static_cast<std::streamsize>(values.size() * sizeof(float)));
  return fileName;
}

fs::path writeScalarImageWithMetadata(const fs::path& dir)
{
  using ImageType = itk::Image<int16_t, 3>;
  using WriterType = itk::ImageFileWriter<ImageType>;

  ImageType::IndexType start;
  ImageType::SizeType size;
  ImageType::PointType origin;
  ImageType::SpacingType spacing;
  start.Fill(0);
  size[0] = 2;
  size[1] = 2;
  size[2] = 1;
  origin[0] = -2.0;
  origin[1] = 4.0;
  origin[2] = 8.0;
  spacing[0] = 0.5;
  spacing[1] = 1.5;
  spacing[2] = 2.5;

  ImageType::DirectionType direction;
  direction.SetIdentity();
  direction(0, 0) = 0.0;
  direction(0, 1) = -1.0;
  direction(1, 0) = 1.0;
  direction(1, 1) = 0.0;

  ImageType::RegionType region;
  region.SetIndex(start);
  region.SetSize(size);

  ImageType::Pointer image = ImageType::New();
  image->SetRegions(region);
  image->SetOrigin(origin);
  image->SetSpacing(spacing);
  image->SetDirection(direction);
  image->Allocate();

  const std::array<int16_t, 4> values{1, 2, 3, 4};
  std::copy(values.begin(), values.end(), image->GetBufferPointer());

  itk::EncapsulateMetaData<std::string>(image->GetMetaDataDictionary(), "entropy_string", "hello\001 world");
  itk::EncapsulateMetaData<int>(image->GetMetaDataDictionary(), "entropy_int", 17);
  itk::EncapsulateMetaData<double>(image->GetMetaDataDictionary(), "entropy_double", 2.5);

  const fs::path fileName = dir / "metadata.nrrd";
  WriterType::Pointer writer = WriterType::New();
  writer->SetFileName(fileName.string());
  writer->SetInput(image);
  writer->UseCompressionOff();
  writer->Update();
  return fileName;
}

template<typename T>
std::vector<T> arithmeticValues(std::size_t count)
{
  std::vector<T> values;
  values.reserve(count);
  for (std::size_t i = 0; i < count; ++i) {
    if constexpr (std::is_floating_point_v<T>) {
      values.push_back(static_cast<T>(0.25 + static_cast<double>(i)));
    }
    else if constexpr (std::is_signed_v<T>) {
      values.push_back(static_cast<T>(static_cast<int64_t>(i) - 2));
    }
    else {
      values.push_back(static_cast<T>(i + 1));
    }
  }
  return values;
}

InterpolationMode expectedDefaultInterpolationMode(ComponentType memoryComponentType)
{
  (void)memoryComponentType;
  return InterpolationMode::Linear;
}

template<typename T>
void checkScalarImageRoundTrip(
  const fs::path& dir,
  const std::string& name,
  ComponentType expectedFileType,
  ComponentType expectedMemoryType)
{
  const std::array<std::size_t, 3> dims{2, 3, 2};
  const std::array<double, 3> spacing{0.5, 1.25, 2.5};
  const std::array<double, 3> origin{-1.0, 2.0, 4.0};
  const std::vector<T> values = arithmeticValues<T>(12);
  const fs::path fileName = writeScalarImage<T, 3>(dir, name, dims, spacing, origin, values);

  Image image(fileName, Rep::Image, BufferType::SeparateImages);

  CHECK(image.loadState() == Image::LoadState::LoadedPixels);
  CHECK(image.hasPixelData());
  CHECK(image.imageRep() == Rep::Image);
  CHECK(image.bufferType() == BufferType::SeparateImages);
  CHECK(image.header().pixelType() == PixelType::Scalar);
  CHECK(image.header().fileComponentType() == expectedFileType);
  CHECK(image.header().memoryComponentType() == expectedMemoryType);
  CHECK(image.header().numComponentsPerPixel() == 1);
  CHECK(image.header().numPixels() == values.size());
  CHECK(image.header().pixelDimensions() == glm::uvec3(2, 3, 2));
  CHECK(image.header().spacing().x == Catch::Approx(spacing[0]));
  CHECK(image.header().spacing().y == Catch::Approx(spacing[1]));
  CHECK(image.header().spacing().z == Catch::Approx(spacing[2]));
  CHECK(image.header().origin().x == Catch::Approx(origin[0]));
  CHECK(image.header().origin().y == Catch::Approx(origin[1]));
  CHECK(image.header().origin().z == Catch::Approx(origin[2]));

  for (std::size_t i = 0; i < values.size(); ++i) {
    CHECK(image.value<double>(0, i).value() == Catch::Approx(static_cast<double>(values[i])));
  }

  const auto stats = image.settings().componentStatistics(0).onlineStats;
  CHECK(stats.count == values.size());
  CHECK(
    static_cast<double>(stats.min) ==
    Catch::Approx(static_cast<double>(*std::min_element(values.begin(), values.end()))));
  CHECK(
    static_cast<double>(stats.max) ==
    Catch::Approx(static_cast<double>(*std::max_element(values.begin(), values.end()))));
  CHECK(image.settings().interpolationMode() == expectedDefaultInterpolationMode(expectedMemoryType));
  CHECK(image.settings().colorInterpolationMode() == expectedDefaultInterpolationMode(expectedMemoryType));

  CHECK(image.generateSortedBuffers());
  CHECK(
    image.quantileToValue(0, 0.0) ==
    Catch::Approx(static_cast<double>(*std::min_element(values.begin(), values.end()))));
  CHECK(
    image.quantileToValue(0, 1.0) ==
    Catch::Approx(static_cast<double>(*std::max_element(values.begin(), values.end()))));
}

} // namespace

TEST_CASE("Scalar images round-trip through disk for supported component types", "[image][loading][io]")
{
  const fs::path dir = testDirectory();

  checkScalarImageRoundTrip<int8_t>(dir, "scalar-int8", ComponentType::Int8, ComponentType::Int8);
  checkScalarImageRoundTrip<uint8_t>(dir, "scalar-uint8", ComponentType::UInt8, ComponentType::UInt8);
  checkScalarImageRoundTrip<int16_t>(dir, "scalar-int16", ComponentType::Int16, ComponentType::Int16);
  checkScalarImageRoundTrip<uint16_t>(dir, "scalar-uint16", ComponentType::UInt16, ComponentType::UInt16);
  checkScalarImageRoundTrip<int32_t>(dir, "scalar-int32", ComponentType::Int32, ComponentType::Int32);
  checkScalarImageRoundTrip<uint32_t>(dir, "scalar-uint32", ComponentType::UInt32, ComponentType::UInt32);
  checkScalarImageRoundTrip<float>(dir, "scalar-float32", ComponentType::Float32, ComponentType::Float32);
}

TEST_CASE("ITK ImageIO metadata extraction populates Entropy IO info", "[image][io-info][itk]")
{
  const fs::path dir = testDirectory();
  const fs::path fileName = writeScalarImageWithMetadata(dir);

  auto imageIo = createStandardImageIo(fileName.string().c_str());
  REQUIRE(imageIo);

  ImageIoInfo info;
  REQUIRE(setImageIoInfoFromItk(info, imageIo));
  CHECK(info.validate());
  CHECK(info.m_fileInfo.m_fileName == fileName);
  CHECK_FALSE(info.m_fileInfo.m_fileTypeString.empty());
  CHECK_FALSE(info.m_fileInfo.m_supportedReadExtensions.empty());
  CHECK_FALSE(info.m_fileInfo.m_supportedWriteExtensions.empty());
  CHECK(info.m_componentInfo.m_componentType == ComponentType::Int16);
  CHECK(info.m_componentInfo.m_componentSizeInBytes == sizeof(int16_t));
  CHECK(info.m_pixelInfo.m_pixelType == PixelType::Scalar);
  CHECK(info.m_pixelInfo.m_numComponents == 1);
  CHECK(info.m_pixelInfo.m_pixelStrideInBytes == sizeof(int16_t));
  CHECK(info.m_sizeInfo.m_imageSizeInPixels == 4);
  CHECK(info.m_sizeInfo.m_imageSizeInComponents == 4);
  CHECK(info.m_sizeInfo.m_imageSizeInBytes == 4 * sizeof(int16_t));
  CHECK(info.m_spaceInfo.m_numDimensions == 3);
  CHECK(info.m_spaceInfo.m_dimensions == std::vector<std::size_t>{2, 2, 1});
  CHECK(info.m_spaceInfo.m_origin.at(0) == Catch::Approx(-2.0));
  CHECK(info.m_spaceInfo.m_spacing.at(2) == Catch::Approx(2.5));
  CHECK(info.m_spaceInfo.m_directions.at(0).at(1) == Catch::Approx(1.0));
  CHECK(info.m_spaceInfo.m_directions.at(1).at(0) == Catch::Approx(-1.0));

  REQUIRE(info.m_metaData.contains("entropy_string"));
  CHECK(std::get<std::string>(info.m_metaData.at("entropy_string")) == "hello world");
  REQUIRE(info.m_metaData.contains("entropy_int"));
  REQUIRE(info.m_metaData.contains("entropy_double"));

  Image image(fileName, Image::ImageRepresentation::Image, Image::MultiComponentBufferType::SeparateImages);
  REQUIRE(image.header().metaData().contains("entropy_string"));
  CHECK(std::get<std::string>(image.header().metaData().at("entropy_string")) == "hello world");
}

TEST_CASE("ITK ImageIO helpers handle null inputs and image-base geometry", "[image][io-info][itk]")
{
  CHECK_FALSE(createStandardImageIo("/definitely/not/a/real/file.nrrd"));

  ImageIoInfo info;
  itk::ImageIOBase::Pointer nullImageIo;
  CHECK_FALSE(setImageIoInfoFromItk(info, nullImageIo));

  SizeInfo sizeInfo;
  itk::ImageBase<3>::Pointer nullImageBase;
  CHECK_FALSE(setSizeInfoFromItkImageBase(sizeInfo, nullImageBase, sizeof(float)));

  SpaceInfo spaceInfo;
  CHECK_FALSE(setSpaceInfoFromItkImageBase(spaceInfo, nullImageBase));

  using ImageType = itk::Image<float, 3>;
  ImageType::Pointer image = ImageType::New();

  ImageType::IndexType start;
  ImageType::SizeType size;
  start.Fill(0);
  size[0] = 3;
  size[1] = 2;
  size[2] = 1;
  ImageType::RegionType region;
  region.SetIndex(start);
  region.SetSize(size);
  image->SetRegions(region);
  image->SetNumberOfComponentsPerPixel(1);

  ImageType::PointType origin;
  origin[0] = 1.0;
  origin[1] = 2.0;
  origin[2] = 3.0;
  ImageType::SpacingType spacing;
  spacing[0] = 0.75;
  spacing[1] = 1.25;
  spacing[2] = 1.75;
  ImageType::DirectionType direction;
  direction.SetIdentity();
  direction(0, 0) = 0.0;
  direction(0, 1) = 1.0;
  direction(1, 0) = -1.0;
  direction(1, 1) = 0.0;
  image->SetOrigin(origin);
  image->SetSpacing(spacing);
  image->SetDirection(direction);

  REQUIRE(setSizeInfoFromItkImageBase(sizeInfo, image.GetPointer(), sizeof(float)));
  CHECK(sizeInfo.m_imageSizeInPixels == 6);
  CHECK(sizeInfo.m_imageSizeInComponents == 6);
  CHECK(sizeInfo.m_imageSizeInBytes == 6 * sizeof(float));

  REQUIRE(setSpaceInfoFromItkImageBase(spaceInfo, image.GetPointer()));
  CHECK(spaceInfo.validate());
  CHECK(spaceInfo.m_dimensions == std::vector<std::size_t>{3, 2, 1});
  CHECK(spaceInfo.m_origin.at(2) == Catch::Approx(3.0));
  CHECK(spaceInfo.m_spacing.at(1) == Catch::Approx(1.25));
  CHECK(spaceInfo.m_directions.at(0).at(1) == Catch::Approx(-1.0));
  CHECK(spaceInfo.m_directions.at(1).at(0) == Catch::Approx(1.0));
}

TEST_CASE("Scalar double images load as float32 in memory", "[image][loading][io][cast]")
{
  const fs::path dir = testDirectory();
  checkScalarImageRoundTrip<double>(dir, "scalar-float64", ComponentType::Float64, ComponentType::Float32);
}

TEST_CASE("Scalar 1D and 2D images load into Entropy's 3D image model", "[image][loading][io]")
{
  const fs::path dir = testDirectory();

  const fs::path oneD = writeScalarImage<uint16_t, 1>(dir, "scalar-1d", {4}, {1.5}, {2.0}, {1, 2, 3, 4});
  const auto oneDHeader = readImageHeaderOnly(oneD, Rep::Image, BufferType::SeparateImages);
  REQUIRE(oneDHeader.has_value());
  CHECK(oneDHeader->pixelDimensions() == glm::uvec3(4, 1, 1));
  Image oneDImage(oneD, Rep::Image, BufferType::SeparateImages);
  CHECK(oneDImage.header().pixelDimensions() == glm::uvec3(4, 1, 1));
  CHECK(oneDImage.header().spacing() == glm::vec3(1.5f, 1.0f, 1.0f));
  CHECK(oneDImage.value<double>(0, 3).value() == Catch::Approx(4.0));

  const fs::path twoD =
    writeScalarImage<int16_t, 2>(dir, "scalar-2d", {2, 3}, {0.75, 1.25}, {-2.0, 5.0}, {-2, -1, 0, 1, 2, 3});
  const auto twoDHeader = readImageHeaderOnly(twoD, Rep::Image, BufferType::SeparateImages);
  REQUIRE(twoDHeader.has_value());
  CHECK(twoDHeader->pixelDimensions() == glm::uvec3(2, 3, 1));
  Image twoDImage(twoD, Rep::Image, BufferType::SeparateImages);
  CHECK(twoDImage.header().pixelDimensions() == glm::uvec3(2, 3, 1));
  CHECK(twoDImage.header().spacing() == glm::vec3(0.75f, 1.25f, 1.0f));
  CHECK(twoDImage.value<double>(0, 1, 2, 0).value() == Catch::Approx(3.0));
}

TEST_CASE(
  "Segmentation loading converts unsupported component types to unsigned integer labels",
  "[image][loading][segmentation]")
{
  const fs::path dir = testDirectory();
  const fs::path fileName =
    writeScalarImage<int16_t, 3>(dir, "segmentation-int16", {2, 2, 1}, {1.0, 1.0, 1.0}, {0.0, 0.0, 0.0}, {-1, 0, 2, 5});

  Image segmentation(fileName, Rep::Segmentation, BufferType::SeparateImages);

  CHECK(segmentation.imageRep() == Rep::Segmentation);
  CHECK(segmentation.header().fileComponentType() == ComponentType::Int16);
  CHECK(segmentation.header().memoryComponentType() == ComponentType::UInt16);
  CHECK(segmentation.settings().interpolationMode() == InterpolationMode::NearestNeighbor);
  CHECK(segmentation.value<double>(0, 0).value() == Catch::Approx(0.0));
  CHECK(segmentation.value<double>(0, 2).value() == Catch::Approx(2.0));
}

TEST_CASE("Vector images can be loaded as separate component buffers", "[image][loading][vector]")
{
  const fs::path dir = testDirectory();
  const fs::path fileName = writeVectorImage<uint16_t>(
    dir,
    "vector-separate",
    {2, 2, 1},
    {1.0, 2.0, 3.0},
    {4.0, 5.0, 6.0},
    3,
    {10, 20, 30, 11, 21, 31, 12, 22, 32, 13, 23, 33});

  Image image(fileName, Rep::Image, BufferType::SeparateImages);

  CHECK(image.header().pixelType() == PixelType::Vector);
  CHECK(image.header().numComponentsPerPixel() == 3);
  CHECK_FALSE(image.header().interleavedComponents());
  CHECK(image.value<double>(0, 2).value() == Catch::Approx(12.0));
  CHECK(image.value<double>(1, 2).value() == Catch::Approx(22.0));
  CHECK(image.value<double>(2, 2).value() == Catch::Approx(32.0));
}

TEST_CASE("Vector images can be loaded as one interleaved component buffer", "[image][loading][vector]")
{
  const fs::path dir = testDirectory();
  const fs::path fileName = writeVectorImage<uint16_t>(
    dir,
    "vector-interleaved",
    {2, 2, 1},
    {1.0, 1.0, 1.0},
    {0.0, 0.0, 0.0},
    3,
    {10, 20, 30, 11, 21, 31, 12, 22, 32, 13, 23, 33});

  Image image(fileName, Rep::Image, BufferType::InterleavedImage);

  CHECK(image.header().numComponentsPerPixel() == 3);
  CHECK(image.header().interleavedComponents());
  CHECK(image.bufferAsVoid(0) != nullptr);
  CHECK(image.bufferAsVoid(1) == nullptr);
  CHECK(image.value<double>(0, 2).value() == Catch::Approx(12.0));
  CHECK(image.value<double>(1, 2).value() == Catch::Approx(22.0));
  CHECK(image.value<double>(2, 2).value() == Catch::Approx(32.0));

  CHECK(image.generateSortedBuffers());
  CHECK(image.quantileToValue(0, 0.0) == Catch::Approx(10.0));
  CHECK(image.quantileToValue(1, 0.0) == Catch::Approx(20.0));
  CHECK(image.quantileToValue(2, 0.0) == Catch::Approx(30.0));
}

TEST_CASE("NIfTI vector displacement images load as interleaved vector fields", "[image][loading][vector][nifti]")
{
  const fs::path dir = testDirectory();
  const fs::path fileName = writeVectorImage<float, 3>(
    dir,
    "ants-style-vector-field",
    {2, 2, 1},
    {1.5, 2.0, 2.5},
    {-3.0, 4.0, 5.0},
    3,
    {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f, 9.0f, 10.0f, 11.0f, 12.0f},
    ".nii.gz");

  const auto header = readImageHeaderOnly(fileName, Rep::Image, BufferType::InterleavedImage);
  REQUIRE(header);
  CHECK(header->pixelDimensions() == glm::uvec3{2, 2, 1});
  CHECK(header->numComponentsPerPixel() == 3);
  CHECK(header->interleavedComponents());

  Image image(fileName, Rep::Image, BufferType::InterleavedImage);

  CHECK(image.header().pixelDimensions() == glm::uvec3{2, 2, 1});
  CHECK(image.header().numComponentsPerPixel() == 3);
  CHECK(image.header().interleavedComponents());
  CHECK(image.bufferAsVoid(0) != nullptr);
  CHECK(image.bufferAsVoid(1) == nullptr);
  CHECK(image.timeAxis().numTimePoints() == 1);
  CHECK(image.value<double>(0, 0).value() == Catch::Approx(1.0));
  CHECK(image.value<double>(1, 0).value() == Catch::Approx(2.0));
  CHECK(image.value<double>(2, 0).value() == Catch::Approx(3.0));
  CHECK(image.value<double>(0, 3).value() == Catch::Approx(10.0));
  CHECK(image.value<double>(1, 3).value() == Catch::Approx(11.0));
  CHECK(image.value<double>(2, 3).value() == Catch::Approx(12.0));
}

TEST_CASE("4D scalar images load as time series", "[image][loading][time]")
{
  const fs::path dir = testDirectory();
  const fs::path fileName = writeScalarImage<float, 4>(
    dir,
    "scalar-time",
    {2, 2, 1, 3},
    {1.0, 2.0, 3.0, 4.0},
    {0.0, 0.0, 0.0, 10.0},
    {0.0f, 1.0f, 2.0f, 3.0f, 10.0f, 11.0f, 12.0f, 13.0f, 20.0f, 21.0f, 22.0f, 23.0f});

  Image image(fileName, Rep::Image, BufferType::SeparateImages);

  CHECK(image.isTimeSeries());
  CHECK(image.timeAxis().numTimePoints() == 3);
  CHECK(image.timeAxis().value(0).value() == Catch::Approx(10.0));
  CHECK(image.timeAxis().value(2).value() == Catch::Approx(18.0));
  CHECK(image.header().pixelDimensions() == glm::uvec3{2, 2, 1});
  CHECK(image.header().numPixels() == 4);
  CHECK(image.value<double>(0, 0, 0).value() == Catch::Approx(0.0));
  CHECK(image.value<double>(0, 0, 1).value() == Catch::Approx(10.0));
  CHECK(image.value<double>(0, 3, 2).value() == Catch::Approx(23.0));
  CHECK(image.value<double>(0, 1, 1, 0, 2).value() == Catch::Approx(23.0));
  CHECK(image.valueLinear<double>(0, 1.0, 1.0, 0.0, 2).value() == Catch::Approx(23.0));
  CHECK(image.bufferAsVoid(0, 3) == nullptr);
  CHECK(image.settings().minMaxImageRange(0).first == Catch::Approx(0.0));
  CHECK(image.settings().minMaxImageRange(0).second == Catch::Approx(23.0));
  CHECK(image.bufferAsVoid(0, 2) != nullptr);
}

TEST_CASE("4D MetaImage scalar images load as time series", "[image][loading][time][mha]")
{
  const fs::path dir = testDirectory();
  const fs::path fileName = writeScalarImage<float, 4>(
    dir,
    "scalar-time",
    {2, 2, 1, 3},
    {1.0, 2.0, 3.0, 1.25},
    {0.0, 0.0, 0.0, 0.0},
    {0.0f, 1.0f, 2.0f, 3.0f, 10.0f, 11.0f, 12.0f, 13.0f, 20.0f, 21.0f, 22.0f, 23.0f},
    ".mha");

  Image image(fileName, Rep::Image, BufferType::SeparateImages);

  CHECK(image.isTimeSeries());
  CHECK(image.timeAxis().numTimePoints() == 3);
  CHECK(image.timeAxis().value(0).value() == Catch::Approx(0.0));
  CHECK(image.timeAxis().value(2).value() == Catch::Approx(2.5));
  CHECK(image.header().pixelDimensions() == glm::uvec3{2, 2, 1});
  CHECK(image.header().numPixels() == 4);
  CHECK(image.value<double>(0, 0, 0).value() == Catch::Approx(0.0));
  CHECK(image.value<double>(0, 3, 2).value() == Catch::Approx(23.0));
}

TEST_CASE("4D vector images preserve interleaved frame offsets", "[image][loading][time][vector]")
{
  const fs::path dir = testDirectory();
  const fs::path fileName = writeVectorImage<float, 4>(
    dir,
    "vector-time",
    {2, 1, 1, 2},
    {1.0, 1.0, 1.0, 1.0},
    {0.0, 0.0, 0.0, 0.0},
    3,
    {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 11.0f, 12.0f, 13.0f, 14.0f, 15.0f, 16.0f});

  Image image(fileName, Rep::Image, BufferType::InterleavedImage);

  CHECK(image.isTimeSeries());
  CHECK(image.header().pixelDimensions() == glm::uvec3{2, 1, 1});
  CHECK(image.header().numComponentsPerPixel() == 3);
  CHECK(image.bufferAsVoid(0, 1) != nullptr);
  CHECK(image.bufferAsVoid(1, 1) == nullptr);
  CHECK(image.value<double>(0, 0, 0).value() == Catch::Approx(1.0));
  CHECK(image.value<double>(2, 1, 0).value() == Catch::Approx(6.0));
  CHECK(image.value<double>(0, 0, 1).value() == Catch::Approx(11.0));
  CHECK(image.value<double>(2, 1, 1).value() == Catch::Approx(16.0));
  CHECK(image.bufferAsVoid(0, 2) == nullptr);

  CHECK(image.generateSortedBuffers());
  CHECK(image.quantileToValue(0, 0.0) == Catch::Approx(1.0));
  CHECK(image.quantileToValue(0, 1.0) == Catch::Approx(14.0));
  CHECK(image.quantileToValue(2, 0.0) == Catch::Approx(3.0));
  CHECK(image.quantileToValue(2, 1.0) == Catch::Approx(16.0));
}

TEST_CASE("4D MetaImage vector image headers load as spatial time series", "[image][loading][time][vector][mhd]")
{
  const fs::path dir = testDirectory();
  const fs::path fileName = writeVectorImage<float, 4>(
    dir,
    "vector-time",
    {2, 1, 1, 2},
    {1.0, 2.0, 3.0, 1.25},
    {0.0, 0.0, 0.0, 0.0},
    3,
    {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 11.0f, 12.0f, 13.0f, 14.0f, 15.0f, 16.0f},
    ".mhd");

  const auto header = readImageHeaderOnly(fileName, Rep::Image, BufferType::InterleavedImage);
  REQUIRE(header);
  CHECK(header->pixelDimensions() == glm::uvec3{2, 1, 1});
  CHECK(header->numPixels() == 2);
  CHECK(header->numComponentsPerPixel() == 3);

  Image image(fileName, Rep::Image, BufferType::InterleavedImage);

  CHECK(image.isTimeSeries());
  CHECK(image.timeAxis().numTimePoints() == 2);
  CHECK(image.timeAxis().value(1).value() == Catch::Approx(1.25));
  CHECK(image.header().pixelDimensions() == glm::uvec3{2, 1, 1});
  CHECK(image.header().numComponentsPerPixel() == 3);
  CHECK(image.value<double>(0, 0, 0).value() == Catch::Approx(1.0));
  CHECK(image.value<double>(2, 1, 1).value() == Catch::Approx(16.0));
}

TEST_CASE("NRRD vector axis images load as multi-component spatial images", "[image][loading][vector][nrrd]")
{
  const fs::path fileName = writeNrrdVectorAxisImage(testDirectory());

  Image image(fileName, Rep::Image, BufferType::SeparateImages);

  CHECK_FALSE(image.isTimeSeries());
  CHECK(image.header().pixelDimensions() == glm::uvec3{2, 2, 1});
  CHECK(image.header().numPixels() == 4);
  CHECK(image.header().numComponentsPerPixel() == 5);
  CHECK(image.value<double>(0, 0).value() == Catch::Approx(0.0));
  CHECK(image.value<double>(4, 0).value() == Catch::Approx(4.0));
  CHECK(image.value<double>(0, 3).value() == Catch::Approx(30.0));
  CHECK(image.value<double>(4, 3).value() == Catch::Approx(34.0));
}

TEST_CASE("NRRD vector axis time images preserve components and time frames", "[image][loading][time][vector][nrrd]")
{
  const fs::path fileName = writeNrrdVectorAxisTimeImage(testDirectory());

  Image image(fileName, Rep::Image, BufferType::SeparateImages);

  CHECK(image.isTimeSeries());
  CHECK(image.timeAxis().numTimePoints() == 3);
  CHECK(image.timeAxis().value(0).value() == Catch::Approx(10.0));
  CHECK(image.timeAxis().value(2).value() == Catch::Approx(14.0));
  CHECK(image.header().pixelDimensions() == glm::uvec3{2, 1, 1});
  CHECK(image.header().numPixels() == 2);
  CHECK(image.header().numComponentsPerPixel() == 2);
  CHECK(image.value<double>(0, 0, 0).value() == Catch::Approx(0.0));
  CHECK(image.value<double>(1, 1, 0).value() == Catch::Approx(11.0));
  CHECK(image.value<double>(0, 0, 2).value() == Catch::Approx(200.0));
  CHECK(image.value<double>(1, 1, 2).value() == Catch::Approx(211.0));
}

TEST_CASE("RGB images load as three component vector images", "[image][loading][rgb]")
{
  const fs::path dir = testDirectory();
  const fs::path fileName = writeRgbImage(dir);

  Image image(fileName, Rep::Image, BufferType::SeparateImages);

  CHECK(image.header().pixelType() == PixelType::RGB);
  CHECK(image.header().numComponentsPerPixel() == 3);
  CHECK(image.value<double>(0, 0).value() == Catch::Approx(1.0));
  CHECK(image.value<double>(1, 0).value() == Catch::Approx(2.0));
  CHECK(image.value<double>(2, 0).value() == Catch::Approx(3.0));
  CHECK(image.value<double>(0, 1).value() == Catch::Approx(4.0));
  CHECK(image.value<double>(1, 1).value() == Catch::Approx(5.0));
  CHECK(image.value<double>(2, 1).value() == Catch::Approx(6.0));
}
