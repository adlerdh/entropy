#include "image/ImageUtility.h"
#include "internal/ImageUtilityItk.h"
#include "internal/ImageUtility.tpp"

#include "common/MathFuncs.h"
#include <spdlog/fmt/std.h>

#include <glm/gtc/epsilon.hpp>
#include <itkImageIOFactory.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <limits>
#include <string_view>
#include <vector>

namespace fs = std::filesystem;

namespace
{

std::string toLowerAscii(std::string_view text)
{
  std::string lower{text};
  std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char ch) {
    return static_cast<char>(std::tolower(ch));
  });
  return lower;
}

bool endsWith(std::string_view text, std::string_view suffix)
{
  return text.size() >= suffix.size() && text.substr(text.size() - suffix.size()) == suffix;
}

std::string stripMedicalImageExtension(std::string fileName)
{
  static constexpr std::array<std::string_view, 29> k_medicalImageExtensions{
    ".nii.gz", ".mhd.gz", ".nhdr.gz", ".hdr.gz", ".img.gz", ".nrrd.gz", ".gipl.gz", ".mnc.gz", ".nii", ".mha",
    ".mhd",    ".nhdr",   ".hdr",     ".img",    ".nrrd",   ".gipl",    ".mnc",     ".mnc2",   ".vtk", ".vti",
    ".hdf5",   ".jpeg",   ".jpg",     ".jpe",    ".png",    ".tiff",    ".tif",     ".bmp",    ".dib"};

  const std::string lowerFileName = toLowerAscii(fileName);
  for (const std::string_view extension : k_medicalImageExtensions) {
    if (endsWith(lowerFileName, extension)) {
      fileName.resize(fileName.size() - extension.size());
      break;
    }
  }

  return fileName;
}

template<typename T>
int sgn(T val)
{
  return (T(0) < val) - (val < T(0));
}

uint32_t componentSizeInBytes(ComponentType type)
{
  switch (type) {
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
    case ComponentType::Float64:
    case ComponentType::Long:
    case ComponentType::ULong:
    case ComponentType::LongLong:
    case ComponentType::ULongLong:
      return 8;
    case ComponentType::LongDouble:
      return sizeof(long double);
    case ComponentType::Undefined:
      return 0;
  }
  return 0;
}

template<typename T>
std::vector<T> copyImageComponentValues(const Image& image, uint32_t component)
{
  const std::size_t numPixels = image.header().numPixels();
  const std::size_t numValues = numPixels * image.timeAxis().numTimePoints();
  std::vector<T> values;
  values.reserve(numValues);

  for (uint32_t timePoint = 0; timePoint < image.timeAxis().numTimePoints(); ++timePoint) {
    for (std::size_t i = 0; i < numPixels; ++i) {
      const auto value = image.value<T>(component, i, timePoint);
      if (!value) {
        spdlog::error("Unable to read image component {} value at index {} and time {}", component, i, timePoint);
        throwDebug("Unable to read image component value");
      }
      values.push_back(*value);
    }
  }

  return values;
}

} // namespace

std::optional<ImageHeader> readImageHeaderOnly(
  const fs::path& fileName,
  Image::ImageRepresentation imageRep,
  Image::MultiComponentBufferType bufferType)
{
  const itk::ImageIOBase::Pointer imageIo = createStandardImageIo(fileName.string().c_str());

  if (!imageIo || imageIo.IsNull()) {
    spdlog::error("Error creating itk::ImageIOBase for image from file {}", fileName);
    return std::nullopt;
  }

  ImageIoInfo ioInfoOnDisk;
  if (!setImageIoInfoFromItk(ioInfoOnDisk, imageIo)) {
    spdlog::error("Error setting image IO information for image from file {}", fileName);
    return std::nullopt;
  }
  normalizeImageIoAxesForEntropy(ioInfoOnDisk, fileName);

  ImageIoInfo ioInfoInMemory = spatializedImageIoInfoForEntropy(ioInfoOnDisk);
  const uint32_t numCompsOnDisk = ioInfoOnDisk.m_pixelInfo.m_numComponents;
  const uint32_t componentsToLoad = componentCountToLoad(imageRep, numCompsOnDisk);

  using CType = ComponentType;
  CType memoryType = ioInfoInMemory.m_componentInfo.m_componentType;

  if (Image::ImageRepresentation::Segmentation == imageRep) {
    switch (memoryType) {
      case CType::UInt8:
      case CType::UInt16:
      case CType::UInt32:
        break;
      case CType::Int8:
        memoryType = CType::UInt8;
        break;
      case CType::Int16:
        memoryType = CType::UInt16;
        break;
      default:
        memoryType = CType::UInt32;
        break;
    }
  }
  else {
    switch (memoryType) {
      case CType::ULong:
      case CType::ULongLong:
        memoryType = CType::UInt32;
        break;
      case CType::Long:
      case CType::LongLong:
        memoryType = CType::Int32;
        break;
      case CType::Float64:
      case CType::LongDouble:
        memoryType = CType::Float32;
        break;
      default:
        break;
    }
  }

  ioInfoInMemory.m_componentInfo.m_componentType = memoryType;
  ioInfoInMemory.m_componentInfo.m_componentTypeString = componentTypeString(memoryType);
  ioInfoInMemory.m_componentInfo.m_componentSizeInBytes = componentSizeInBytes(memoryType);
  ioInfoInMemory.m_pixelInfo.m_numComponents = componentsToLoad;
  ioInfoInMemory.m_sizeInfo.m_imageSizeInComponents =
    ioInfoInMemory.m_sizeInfo.m_imageSizeInPixels * componentsToLoad * ioInfoOnDisk.m_timeInfo.m_numTimePoints;
  ioInfoInMemory.m_sizeInfo.m_imageSizeInBytes =
    ioInfoInMemory.m_sizeInfo.m_imageSizeInComponents * ioInfoInMemory.m_componentInfo.m_componentSizeInBytes;

  ImageHeader header(ioInfoOnDisk, ioInfoInMemory, Image::MultiComponentBufferType::InterleavedImage == bufferType);
  header.setNumComponentsPerPixel(componentsToLoad);
  return header;
}

uint32_t componentCountToLoad(ImageRepresentation imageRep, uint32_t sourceComponents)
{
  if (0u == sourceComponents) {
    return 0u;
  }
  if (ImageRepresentation::Segmentation == imageRep) {
    return 1u;
  }
  return sourceComponents;
}

std::string getFileName(const std::string& filePath, bool withExtension)
{
  const std::string fileName = fs::path(filePath).filename().string();
  if (withExtension) {
    return fileName;
  }

  return stripMedicalImageExtension(fileName);
}

bool isStandardRasterImageFile(const fs::path& fileName)
{
  const std::string extension = toLowerAscii(fileName.extension().string());
  return extension == ".jpg" || extension == ".jpeg" || extension == ".jpe" || extension == ".png" ||
         extension == ".tif" || extension == ".tiff" || extension == ".bmp" || extension == ".dib";
}

bool isStandardRasterColorImage(const ImageHeader& header)
{
  const bool colorComponentCount = (PixelType::RGB == header.pixelType() && 3u == header.numComponentsPerPixel()) ||
                                   (PixelType::RGBA == header.pixelType() && 4u == header.numComponentsPerPixel());
  return isStandardRasterImageFile(header.fileName()) && countNonSingletonPixelDimensions(header) <= 2u &&
         colorComponentCount;
}

std::string imageFormatName(const fs::path& fileName)
{
  const std::string extension = toLowerAscii(fileName.extension().string());
  const std::string lowerName = toLowerAscii(fileName.string());
  if (extension == ".dcm") return "DICOM";
  if (
    extension == ".nrrd" || extension == ".nhdr" || endsWith(lowerName, ".nrrd.gz") || endsWith(lowerName, ".nhdr.gz"))
    return "NRRD";
  if (extension == ".nii" || endsWith(lowerName, ".nii.gz")) return "NIfTI";
  if (extension == ".mha" || extension == ".mhd" || endsWith(lowerName, ".mhd.gz")) return "MetaImage";
  if (extension == ".hdr" || extension == ".img" || endsWith(lowerName, ".hdr.gz") || endsWith(lowerName, ".img.gz"))
    return "Analyze";
  if (extension == ".jpg" || extension == ".jpeg" || extension == ".jpe") return "JPEG";
  if (extension == ".png") return "PNG";
  if (extension == ".tif" || extension == ".tiff") return "TIFF";
  if (extension == ".bmp" || extension == ".dib") return "BMP";
  if (extension == ".gipl" || endsWith(lowerName, ".gipl.gz")) return "GIPL";
  if (extension == ".mnc" || extension == ".mnc2" || endsWith(lowerName, ".mnc.gz")) return "MINC";
  if (extension == ".vtk" || extension == ".vti") return "VTK";
  if (extension == ".hdf5") return "HDF5";
  return extension.empty() ? "Unknown" : extension.substr(1);
}

PixelType fromItkPixelType(const itk::IOPixelEnum& pixelType)
{
  switch (pixelType) {
    case itk::IOPixelEnum::UNKNOWNPIXELTYPE:
      return PixelType::Undefined;
    case itk::IOPixelEnum::SCALAR:
      return PixelType::Scalar;
    case itk::IOPixelEnum::RGB:
      return PixelType::RGB;
    case itk::IOPixelEnum::RGBA:
      return PixelType::RGBA;
    case itk::IOPixelEnum::OFFSET:
      return PixelType::Offset;
    case itk::IOPixelEnum::VECTOR:
      return PixelType::Vector;
    case itk::IOPixelEnum::POINT:
      return PixelType::Point;
    case itk::IOPixelEnum::COVARIANTVECTOR:
      return PixelType::CovariantVector;
    case itk::IOPixelEnum::SYMMETRICSECONDRANKTENSOR:
      return PixelType::SymmetricSecondRankTensor;
    case itk::IOPixelEnum::DIFFUSIONTENSOR3D:
      return PixelType::DiffusionTensor3D;
    case itk::IOPixelEnum::COMPLEX:
      return PixelType::Complex;
    case itk::IOPixelEnum::FIXEDARRAY:
      return PixelType::FixedArray;
    case itk::IOPixelEnum::ARRAY:
      return PixelType::Array;
    case itk::IOPixelEnum::MATRIX:
      return PixelType::Matrix;
    case itk::IOPixelEnum::VARIABLELENGTHVECTOR:
      return PixelType::VariableLengthVector;
    case itk::IOPixelEnum::VARIABLESIZEMATRIX:
      return PixelType::VariableSizeMatrix;
  }

  return PixelType::Undefined;
}

ComponentType fromItkComponentType(const itk::IOComponentEnum& componentType)
{
  switch (componentType) {
    case itk::IOComponentEnum::UCHAR:
      return ComponentType::UInt8;
    case itk::IOComponentEnum::CHAR:
      return ComponentType::Int8;
    case itk::IOComponentEnum::USHORT:
      return ComponentType::UInt16;
    case itk::IOComponentEnum::SHORT:
      return ComponentType::Int16;
    case itk::IOComponentEnum::UINT:
      return ComponentType::UInt32;
    case itk::IOComponentEnum::INT:
      return ComponentType::Int32;
    case itk::IOComponentEnum::FLOAT:
      return ComponentType::Float32;
    case itk::IOComponentEnum::LONG:
      return ComponentType::Long;
    case itk::IOComponentEnum::ULONG:
      return ComponentType::ULong;
    case itk::IOComponentEnum::LONGLONG:
      return ComponentType::LongLong;
    case itk::IOComponentEnum::ULONGLONG:
      return ComponentType::ULongLong;
    case itk::IOComponentEnum::DOUBLE:
      return ComponentType::Float64;
    case itk::IOComponentEnum::LDOUBLE:
      return ComponentType::LongDouble;

    default:
    case itk::IOComponentEnum::UNKNOWNCOMPONENTTYPE:
      return ComponentType::Undefined;
  }
}

itk::IOComponentEnum toItkComponentType(const ComponentType& componentType)
{
  switch (componentType) {
    case ComponentType::Int8:
      return itk::IOComponentEnum::CHAR;
    case ComponentType::UInt8:
      return itk::IOComponentEnum::UCHAR;
    case ComponentType::Int16:
      return itk::IOComponentEnum::SHORT;
    case ComponentType::UInt16:
      return itk::IOComponentEnum::USHORT;
    case ComponentType::Int32:
      return itk::IOComponentEnum::INT;
    case ComponentType::UInt32:
      return itk::IOComponentEnum::UINT;
    case ComponentType::Float32:
      return itk::IOComponentEnum::FLOAT;
    case ComponentType::Float64:
      return itk::IOComponentEnum::DOUBLE;
    case ComponentType::Long:
      return itk::IOComponentEnum::LONG;
    case ComponentType::ULong:
      return itk::IOComponentEnum::ULONG;
    case ComponentType::LongLong:
      return itk::IOComponentEnum::LONGLONG;
    case ComponentType::ULongLong:
      return itk::IOComponentEnum::ULONG;
    case ComponentType::LongDouble:
      return itk::IOComponentEnum::LDOUBLE;

    default:
    case ComponentType::Undefined:
      return itk::IOComponentEnum::UNKNOWNCOMPONENTTYPE;
  }
}

std::pair<itk::CommonEnums::IOComponent, std::string> sniffComponentType(const char* fileName)
{
  static const std::pair<itk::CommonEnums::IOComponent, std::string> UNKNOWN{
    itk::CommonEnums::IOComponent::UNKNOWNCOMPONENTTYPE,
    "UNKNOWNCOMPONENTTYPE"};

  try {
    const itk::ImageIOBase::Pointer imageIO =
      itk::ImageIOFactory::CreateImageIO(fileName, itk::ImageIOFactory::ReadMode);

    if (!imageIO || imageIO.IsNull()) {
      // None of the registered ImageIO classes can read the file
      spdlog::error("ITK image I/O factory could not create the I/O object for image {}", fileName);
      return UNKNOWN;
    }

    imageIO->SetFileName(fileName);
    imageIO->ReadImageInformation();

    const auto compType = imageIO->GetComponentType();
    return {compType, itk::ImageIOBase::GetComponentTypeAsString(compType)};
  }
  catch (const itk::ExceptionObject& e) {
    spdlog::error("Exception while creating ImageIOBase for image {}: {}", fileName, e.what());
    return UNKNOWN;
  }
  catch (...) {
    spdlog::error("Unknown exception while creating ImageIOBase for image {}", fileName);
    return UNKNOWN;
  }
}

typename itk::ImageIOBase::Pointer createStandardImageIo(const char* fileName)
{
  try {
    const itk::ImageIOBase::Pointer imageIo =
      itk::ImageIOFactory::CreateImageIO(fileName, itk::ImageIOFactory::ReadMode);

    if (!imageIo || imageIo.IsNull()) {
      // None of the registered ImageIO classes can read the file
      spdlog::error("ITK image I/O factory could not create the I/O object for image {}", fileName);
      return nullptr;
    }

    imageIo->SetFileName(fileName);
    imageIo->ReadImageInformation();
    return imageIo;
  }
  catch (const itk::ExceptionObject& e) {
    spdlog::error("Exception while creating ImageIOBase for image {}: {}", fileName, e.what());
    return nullptr;
  }
  catch (...) {
    spdlog::error("Unknown exception while creating ImageIOBase for image {}", fileName);
    return nullptr;
  }
}

std::pair<double, double> componentRange(const ComponentType& componentType)
{
  switch (componentType) {
    case ComponentType::Int8:
      return {std::numeric_limits<int8_t>::lowest(), std::numeric_limits<int8_t>::max()};
    case ComponentType::UInt8:
      return {std::numeric_limits<uint8_t>::lowest(), std::numeric_limits<uint8_t>::max()};
    case ComponentType::Int16:
      return {std::numeric_limits<int16_t>::lowest(), std::numeric_limits<int16_t>::max()};
    case ComponentType::UInt16:
      return {std::numeric_limits<uint16_t>::lowest(), std::numeric_limits<uint16_t>::max()};
    case ComponentType::Int32:
      return {std::numeric_limits<int32_t>::lowest(), std::numeric_limits<int32_t>::max()};
    case ComponentType::UInt32:
      return {std::numeric_limits<uint32_t>::lowest(), std::numeric_limits<uint32_t>::max()};
    case ComponentType::Float32:
      return {std::numeric_limits<float>::lowest(), std::numeric_limits<float>::max()};
    default:
      return {0.0, 0.0};
  }
}

std::pair<glm::vec3, glm::vec3> computeWorldMinMaxCornersOfImage(const Image& image)
{
  const auto& world_T_subject = image.transformations().worldDef_T_subject();
  const auto& subjectCorners = image.header().subjectBBoxCorners();

  std::array<glm::vec3, 8> worldCorners;

  std::transform(
    std::begin(subjectCorners),
    std::end(subjectCorners),
    std::begin(worldCorners),
    [&world_T_subject](const glm::dvec3& v) {
      const glm::vec4 worldCorner = world_T_subject * glm::vec4{v, 1.0f};
      return glm::vec3{worldCorner} / worldCorner.w;
    });

  return math::computeMinMaxCornersOfAABBox(worldCorners);
}

std::size_t countNonSingletonPixelDimensions(const ImageHeader& header)
{
  const glm::uvec3& dims = header.pixelDimensions();
  return static_cast<std::size_t>(dims.x > 1u) + static_cast<std::size_t>(dims.y > 1u) +
         static_cast<std::size_t>(dims.z > 1u);
}

bool isLinearOrPlanarImage(const ImageHeader& header)
{
  return countNonSingletonPixelDimensions(header) <= 2;
}

std::size_t computeNumImageSlicesAlongWorldDirection(const Image& image, const glm::vec3& worldDir)
{
  if (glm::dot(worldDir, worldDir) <= glm::epsilon<float>()) {
    return 0;
  }

  const glm::vec3 normalizedWorldDir = glm::normalize(worldDir);
  const glm::mat3 pixel_T_world = glm::mat3{image.transformations().pixel_T_worldDef()};

  glm::vec3 pixelDir = glm::abs(glm::normalize(glm::vec3{pixel_T_world * normalizedWorldDir}));
  const float pixelDirSum = pixelDir.x + pixelDir.y + pixelDir.z;
  if (pixelDirSum <= glm::epsilon<float>()) {
    return 0;
  }
  pixelDir /= pixelDirSum;

  const float spacing = glm::dot(image.header().spacing(), pixelDir);
  if (spacing <= 0.0f) {
    return 0;
  }

  const auto& world_T_subject = image.transformations().worldDef_T_subject();
  const auto& subjectCorners = image.header().subjectBBoxCorners();

  float minDistance = std::numeric_limits<float>::max();
  float maxDistance = std::numeric_limits<float>::lowest();
  for (const glm::vec3& subjectCorner : subjectCorners) {
    const glm::vec4 worldCornerH = world_T_subject * glm::vec4{subjectCorner, 1.0f};
    const glm::vec3 worldCorner = glm::vec3{worldCornerH} / worldCornerH.w;
    const float distance = glm::dot(worldCorner, normalizedWorldDir);
    minDistance = std::min(minDistance, distance);
    maxDistance = std::max(maxDistance, distance);
  }

  return static_cast<std::size_t>(std::ceil((maxDistance - minDistance) / spacing));
}

std::vector<ComponentStats> computeImageStatisticsOnSortedValues(const Image& image)
{
  std::vector<ComponentStats> componentStats;

  const std::size_t N = image.header().numPixels() * image.timeAxis().numTimePoints();

  for (uint32_t i = 0; i < image.header().numComponentsPerPixel(); ++i) {
    const void* bufferSorted = image.bufferSortedAsVoid(i);

    switch (image.header().memoryComponentType()) {
      case ComponentType::Int8: {
        componentStats.emplace_back(
          computeStatsOnSortedBuffers<int8_t>(std::span(static_cast<const int8_t*>(bufferSorted), N)));
        break;
      }
      case ComponentType::UInt8: {
        componentStats.emplace_back(
          computeStatsOnSortedBuffers<uint8_t>(std::span(static_cast<const uint8_t*>(bufferSorted), N)));
        break;
      }
      case ComponentType::Int16: {
        componentStats.emplace_back(
          computeStatsOnSortedBuffers<int16_t>(std::span(static_cast<const int16_t*>(bufferSorted), N)));
        break;
      }
      case ComponentType::UInt16: {
        componentStats.emplace_back(
          computeStatsOnSortedBuffers<uint16_t>(std::span(static_cast<const uint16_t*>(bufferSorted), N)));
        break;
      }
      case ComponentType::Int32: {
        componentStats.emplace_back(
          computeStatsOnSortedBuffers<int32_t>(std::span(static_cast<const int32_t*>(bufferSorted), N)));
        break;
      }
      case ComponentType::UInt32: {
        componentStats.emplace_back(
          computeStatsOnSortedBuffers<uint32_t>(std::span(static_cast<const uint32_t*>(bufferSorted), N)));
        break;
      }
      case ComponentType::Float32: {
        componentStats.emplace_back(
          computeStatsOnSortedBuffers<float>(std::span(static_cast<const float*>(bufferSorted), N)));
        break;
      }
      default: {
        spdlog::error("Invalid component type '{}'", componentTypeString(image.header().memoryComponentType()));
        return componentStats;
      }
    }
  }

  return componentStats;
}

std::vector<OnlineStats> computeImageStatisticsOnUnsortedValues(const Image& image)
{
  std::vector<OnlineStats> componentStats;

  const std::size_t N = image.header().numPixels() * image.timeAxis().numTimePoints();

  for (uint32_t i = 0; i < image.header().numComponentsPerPixel(); ++i) {
    const void* buffer = image.bufferAsVoid(i);

    switch (image.header().memoryComponentType()) {
      case ComponentType::Int8: {
        if (!buffer || image.bufferType() == Image::MultiComponentBufferType::InterleavedImage) {
          const auto values = copyImageComponentValues<int8_t>(image, i);
          componentStats.emplace_back(computeStatsOnUnsortedBuffers<int8_t>(std::span(values)));
          break;
        }
        componentStats.emplace_back(
          computeStatsOnUnsortedBuffers<int8_t>(std::span(static_cast<const int8_t*>(buffer), N)));
        break;
      }
      case ComponentType::UInt8: {
        if (!buffer || image.bufferType() == Image::MultiComponentBufferType::InterleavedImage) {
          const auto values = copyImageComponentValues<uint8_t>(image, i);
          componentStats.emplace_back(computeStatsOnUnsortedBuffers<uint8_t>(std::span(values)));
          break;
        }
        componentStats.emplace_back(
          computeStatsOnUnsortedBuffers<uint8_t>(std::span(static_cast<const uint8_t*>(buffer), N)));
        break;
      }
      case ComponentType::Int16: {
        if (!buffer || image.bufferType() == Image::MultiComponentBufferType::InterleavedImage) {
          const auto values = copyImageComponentValues<int16_t>(image, i);
          componentStats.emplace_back(computeStatsOnUnsortedBuffers<int16_t>(std::span(values)));
          break;
        }
        componentStats.emplace_back(
          computeStatsOnUnsortedBuffers<int16_t>(std::span(static_cast<const int16_t*>(buffer), N)));
        break;
      }
      case ComponentType::UInt16: {
        if (!buffer || image.bufferType() == Image::MultiComponentBufferType::InterleavedImage) {
          const auto values = copyImageComponentValues<uint16_t>(image, i);
          componentStats.emplace_back(computeStatsOnUnsortedBuffers<uint16_t>(std::span(values)));
          break;
        }
        componentStats.emplace_back(
          computeStatsOnUnsortedBuffers<uint16_t>(std::span(static_cast<const uint16_t*>(buffer), N)));
        break;
      }
      case ComponentType::Int32: {
        if (!buffer || image.bufferType() == Image::MultiComponentBufferType::InterleavedImage) {
          const auto values = copyImageComponentValues<int32_t>(image, i);
          componentStats.emplace_back(computeStatsOnUnsortedBuffers<int32_t>(std::span(values)));
          break;
        }
        componentStats.emplace_back(
          computeStatsOnUnsortedBuffers<int32_t>(std::span(static_cast<const int32_t*>(buffer), N)));
        break;
      }
      case ComponentType::UInt32: {
        if (!buffer || image.bufferType() == Image::MultiComponentBufferType::InterleavedImage) {
          const auto values = copyImageComponentValues<uint32_t>(image, i);
          componentStats.emplace_back(computeStatsOnUnsortedBuffers<uint32_t>(std::span(values)));
          break;
        }
        componentStats.emplace_back(
          computeStatsOnUnsortedBuffers<uint32_t>(std::span(static_cast<const uint32_t*>(buffer), N)));
        break;
      }
      case ComponentType::Float32: {
        if (!buffer || image.bufferType() == Image::MultiComponentBufferType::InterleavedImage) {
          const auto values = copyImageComponentValues<float>(image, i);
          componentStats.emplace_back(computeStatsOnUnsortedBuffers<float>(std::span(values)));
          break;
        }
        componentStats.emplace_back(
          computeStatsOnUnsortedBuffers<float>(std::span(static_cast<const float*>(buffer), N)));
        break;
      }
      default: {
        spdlog::error("Invalid image component type '{}'", componentTypeString(image.header().memoryComponentType()));
        return componentStats;
      }
    }
  }

  return componentStats;
}

std::vector<tdigest::TDigest> computeTDigests(const Image& image)
{
  spdlog::debug("Computing T-digest for image intensities");

  const std::size_t N = image.header().numPixels() * image.timeAxis().numTimePoints();
  std::vector<tdigest::TDigest> digests;

  const unsigned int numTh = std::max(std::thread::hardware_concurrency() - 1, 1u);

  for (uint32_t i = 0; i < image.header().numComponentsPerPixel(); ++i) {
    const void* buffer = image.bufferAsVoid(i);

    switch (image.header().memoryComponentType()) {
      case ComponentType::Int8: {
        if (!buffer || image.bufferType() == Image::MultiComponentBufferType::InterleavedImage) {
          const auto values = copyImageComponentValues<int8_t>(image, i);
          digests.emplace_back(buildTDigest<int8_t>(std::span(values), numTh));
          break;
        }
        digests.emplace_back(buildTDigest<int8_t>(std::span(static_cast<const int8_t*>(buffer), N), numTh));
        break;
      }
      case ComponentType::UInt8: {
        if (!buffer || image.bufferType() == Image::MultiComponentBufferType::InterleavedImage) {
          const auto values = copyImageComponentValues<uint8_t>(image, i);
          digests.emplace_back(buildTDigest<uint8_t>(std::span(values), numTh));
          break;
        }
        digests.emplace_back(buildTDigest<uint8_t>(std::span(static_cast<const uint8_t*>(buffer), N), numTh));
        break;
      }
      case ComponentType::Int16: {
        if (!buffer || image.bufferType() == Image::MultiComponentBufferType::InterleavedImage) {
          const auto values = copyImageComponentValues<int16_t>(image, i);
          digests.emplace_back(buildTDigest<int16_t>(std::span(values), numTh));
          break;
        }
        digests.emplace_back(buildTDigest<int16_t>(std::span(static_cast<const int16_t*>(buffer), N), numTh));
        break;
      }
      case ComponentType::UInt16: {
        if (!buffer || image.bufferType() == Image::MultiComponentBufferType::InterleavedImage) {
          const auto values = copyImageComponentValues<uint16_t>(image, i);
          digests.emplace_back(buildTDigest<uint16_t>(std::span(values), numTh));
          break;
        }
        digests.emplace_back(buildTDigest<uint16_t>(std::span(static_cast<const uint16_t*>(buffer), N), numTh));
        break;
      }
      case ComponentType::Int32: {
        if (!buffer || image.bufferType() == Image::MultiComponentBufferType::InterleavedImage) {
          const auto values = copyImageComponentValues<int32_t>(image, i);
          digests.emplace_back(buildTDigest<int32_t>(std::span(values), numTh));
          break;
        }
        digests.emplace_back(buildTDigest<int32_t>(std::span(static_cast<const int32_t*>(buffer), N), numTh));
        break;
      }
      case ComponentType::UInt32: {
        if (!buffer || image.bufferType() == Image::MultiComponentBufferType::InterleavedImage) {
          const auto values = copyImageComponentValues<uint32_t>(image, i);
          digests.emplace_back(buildTDigest<uint32_t>(std::span(values), numTh));
          break;
        }
        digests.emplace_back(buildTDigest<uint32_t>(std::span(static_cast<const uint32_t*>(buffer), N), numTh));
        break;
      }
      case ComponentType::Float32: {
        if (!buffer || image.bufferType() == Image::MultiComponentBufferType::InterleavedImage) {
          const auto values = copyImageComponentValues<float>(image, i);
          digests.emplace_back(buildTDigest<float>(std::span(values), numTh));
          break;
        }
        digests.emplace_back(buildTDigest<float>(std::span(static_cast<const float*>(buffer), N), numTh));
        break;
      }
      default: {
        spdlog::error("Invalid image component type '{}'", componentTypeString(image.header().memoryComponentType()));
        return digests;
      }
    }
  }

  return digests;
}

double bumpQuantile(
  const Image& image,
  uint32_t comp,
  double currentQuantile,
  double attemptedQuantile,
  double currentValue,
  bool usingExactQuantiles)
{
  const int dir = sgn(attemptedQuantile - currentQuantile);
  if (0 == dir) {
    return currentQuantile;
  }

  const std::size_t N = image.header().numPixels();

  double newQuant = attemptedQuantile;
  double oldValue = currentValue;
  double newValue = image.quantileToValue(comp, newQuant);

  while (newValue == currentValue) {
    const QuantileOfValue Q = image.valueToQuantile(comp, oldValue);
    oldValue = newValue;

    if (dir < 0) {
      if (usingExactQuantiles) {
        newQuant = (0 == Q.lowerIndex) ? 0.0 : static_cast<double>(Q.lowerIndex - 1) / N;
      }
      else {
        newQuant = (0 == Q.lowerIndex) ? 0.0 : newQuant - 1.0 / N;
      }
    }
    else if (dir > 0) {
      if (usingExactQuantiles) {
        newQuant = (N == Q.upperIndex) ? 1.0 : static_cast<double>(Q.upperIndex + 1) / N;
      }
      else {
        newQuant = (N == Q.upperIndex) ? 1.0 : newQuant + 1.0 / N;
      }
    }

    newValue = image.quantileToValue(comp, newQuant);

    // The loop should theoretically need to run only once.
    // But more loops may be required if there are some numerical errors.

    if (usingExactQuantiles) {
      break;
    }
  }

  return newQuant;
}

std::optional<std::size_t>
computeNumHistogramBins(const NumBinsComputationMethod& method, std::size_t numPixels, ComponentStats stats)
{
  if (0 == numPixels) {
    spdlog::warn("Cannot compute number of histogram bins for image component with zero pixels");
    return std::nullopt;
  }

  switch (method) {
    case NumBinsComputationMethod::SquareRoot: {
      return static_cast<std::size_t>(std::ceil(std::sqrt(numPixels)));
    }
    case NumBinsComputationMethod::Sturges: {
      return static_cast<std::size_t>(std::ceil(std::log2(numPixels)) + 1);
    }
    case NumBinsComputationMethod::Rice: {
      return static_cast<std::size_t>(std::ceil(2.0 * std::pow(numPixels, 1.0 / 3.0)));
    }
    case NumBinsComputationMethod::Scott: {
      if (glm::epsilonEqual(static_cast<double>(stats.onlineStats.stdev), 0.0, glm::epsilon<double>())) {
        spdlog::debug("Image component has zero standard deviation");
        return std::nullopt;
      }

      const double binWidth = 3.49 * stats.onlineStats.stdev / std::pow(numPixels, 1.0 / 3.0);
      return static_cast<std::size_t>(std::ceil((stats.onlineStats.max - stats.onlineStats.min) / binWidth));
    }
    case NumBinsComputationMethod::FreedmanDiaconis: {
      const double IQR = (stats.quantiles[75] - stats.quantiles[25]);
      if (glm::epsilonEqual(IQR, 0.0, glm::epsilon<double>())) {
        spdlog::debug("Image component has zero interquartile range");
        return std::nullopt;
      }

      const double binWidth = 2.0 * IQR / std::pow(numPixels, 1.0 / 3.0);
      return static_cast<std::size_t>(std::ceil((stats.onlineStats.max - stats.onlineStats.min) / binWidth));
    }
  }

  return std::nullopt;
}
