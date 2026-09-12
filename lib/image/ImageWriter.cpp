#include "image/ImageWriter.h"

#include "image/Image.h"
#include "image/ImageHeader.h"
#include "image/ImageTimeAxis.h"
#include "image/ImageUtility.h"
#include "image/internal/ImageUtilityItk.h"

#include <itkImageIOBase.h>
#include <itkImageIOFactory.h>
#include <itkImageIORegion.h>
#include <itkMetaDataObject.h>

#include <cstddef>
#include <cmath>
#include <cstring>
#include <limits>
#include <string>
#include <utility>
#include <vector>

namespace
{
image_io::WriteResult failure(image_io::WriteError error, std::string message)
{
  return {.error = error, .message = std::move(message)};
}

itk::IOPixelEnum itkPixelType(PixelType pixelType, std::uint32_t numComponents)
{
  if (1u == numComponents) {
    return itk::IOPixelEnum::SCALAR;
  }

  switch (pixelType) {
    case PixelType::RGB:
      return 3u == numComponents ? itk::IOPixelEnum::RGB : itk::IOPixelEnum::VECTOR;
    case PixelType::RGBA:
      return 4u == numComponents ? itk::IOPixelEnum::RGBA : itk::IOPixelEnum::VECTOR;
    case PixelType::Offset:
      return itk::IOPixelEnum::OFFSET;
    case PixelType::Point:
      return itk::IOPixelEnum::POINT;
    case PixelType::CovariantVector:
      return itk::IOPixelEnum::COVARIANTVECTOR;
    case PixelType::SymmetricSecondRankTensor:
      return itk::IOPixelEnum::SYMMETRICSECONDRANKTENSOR;
    case PixelType::DiffusionTensor3D:
      return itk::IOPixelEnum::DIFFUSIONTENSOR3D;
    case PixelType::Complex:
      return 2u == numComponents ? itk::IOPixelEnum::COMPLEX : itk::IOPixelEnum::VECTOR;
    case PixelType::FixedArray:
      return itk::IOPixelEnum::FIXEDARRAY;
    case PixelType::Array:
      return itk::IOPixelEnum::ARRAY;
    case PixelType::Matrix:
      return itk::IOPixelEnum::MATRIX;
    case PixelType::VariableLengthVector:
      return itk::IOPixelEnum::VARIABLELENGTHVECTOR;
    case PixelType::VariableSizeMatrix:
      return itk::IOPixelEnum::VARIABLESIZEMATRIX;
    case PixelType::Scalar:
    case PixelType::Vector:
    case PixelType::Undefined:
      return itk::IOPixelEnum::VECTOR;
  }

  return itk::IOPixelEnum::VECTOR;
}

bool checkedMultiply(std::size_t lhs, std::size_t rhs, std::size_t& result)
{
  if (0u != lhs && rhs > std::numeric_limits<std::size_t>::max() / lhs) {
    return false;
  }
  result = lhs * rhs;
  return true;
}

image_io::WriteResult copySelectedPixels(
  const Image& image,
  const image_io::WriteOptions& options,
  std::size_t componentBytes,
  std::vector<std::byte>& scratch,
  const void*& output)
{
  const std::uint32_t firstComponent = options.component.value_or(0u);
  const std::uint32_t outputComponents = options.component ? 1u : image.header().numComponentsPerPixel();
  const std::uint32_t firstTimePoint = options.timePoint.value_or(0u);
  const std::uint32_t outputTimePoints = options.timePoint ? 1u : image.timeAxis().numTimePoints();
  const std::size_t spatialPixels = image.header().numPixels();

  std::size_t outputElements = 0u;
  std::size_t outputBytes = 0u;
  if (
    !checkedMultiply(spatialPixels, outputTimePoints, outputElements) ||
    !checkedMultiply(outputElements, outputComponents, outputElements) ||
    !checkedMultiply(outputElements, componentBytes, outputBytes))
  {
    return failure(image_io::WriteError::InvalidImageData, "The image is too large to export on this system.");
  }
  const bool sourceIsInterleaved = Image::MultiComponentBufferType::InterleavedImage == image.bufferType();
  if ((sourceIsInterleaved && !options.component) || (!sourceIsInterleaved && 1u == outputComponents)) {
    const std::uint32_t bufferIndex = sourceIsInterleaved ? 0u : firstComponent;
    output = image.bufferAsVoid(bufferIndex, firstTimePoint);
    if (!output) {
      return failure(image_io::WriteError::InvalidImageData, "An image pixel buffer is missing.");
    }
    return {};
  }

  scratch.resize(outputBytes);

  std::byte* destination = scratch.data();
  for (std::uint32_t outputTime = 0u; outputTime < outputTimePoints; ++outputTime) {
    const std::uint32_t sourceTime = firstTimePoint + outputTime;
    if (Image::MultiComponentBufferType::InterleavedImage == image.bufferType()) {
      const auto* source = static_cast<const std::byte*>(image.bufferAsVoid(0u, sourceTime));
      if (!source) {
        return failure(image_io::WriteError::InvalidImageData, "An interleaved image buffer is missing.");
      }

      if (!options.component) {
        const std::size_t frameBytes = spatialPixels * outputComponents * componentBytes;
        std::memcpy(destination, source, frameBytes);
        destination += frameBytes;
        continue;
      }

      const std::size_t sourcePixelBytes = image.header().numComponentsPerPixel() * componentBytes;
      for (std::size_t pixel = 0u; pixel < spatialPixels; ++pixel) {
        const std::byte* value = source + pixel * sourcePixelBytes + firstComponent * componentBytes;
        std::memcpy(destination, value, componentBytes);
        destination += componentBytes;
      }
      continue;
    }

    std::vector<const std::byte*> componentBuffers;
    componentBuffers.reserve(outputComponents);
    for (std::uint32_t outputComponent = 0u; outputComponent < outputComponents; ++outputComponent) {
      const auto* buffer =
        static_cast<const std::byte*>(image.bufferAsVoid(firstComponent + outputComponent, sourceTime));
      if (!buffer) {
        return failure(image_io::WriteError::InvalidImageData, "A separated image component buffer is missing.");
      }
      componentBuffers.push_back(buffer);
    }

    for (std::size_t pixel = 0u; pixel < spatialPixels; ++pixel) {
      for (const std::byte* componentBuffer : componentBuffers) {
        std::memcpy(destination, componentBuffer + pixel * componentBytes, componentBytes);
        destination += componentBytes;
      }
    }
  }

  output = scratch.data();
  return {};
}

void configureSpatialAxes(
  itk::ImageIOBase& imageIo,
  const ImageHeader& header,
  std::uint32_t spatialDimensions,
  std::uint32_t outputDimensions)
{
  const glm::uvec3& dimensions = header.pixelDimensions();
  const glm::vec3& origin = header.origin();
  const glm::vec3& spacing = header.spacing();
  const glm::mat3& directions = header.directions();

  for (std::uint32_t axis = 0u; axis < spatialDimensions; ++axis) {
    imageIo.SetDimensions(axis, dimensions[axis]);
    imageIo.SetOrigin(axis, origin[axis]);
    imageIo.SetSpacing(axis, spacing[axis]);

    std::vector<double> direction(outputDimensions, 0.0);
    for (std::uint32_t component = 0u; component < spatialDimensions; ++component) {
      direction[component] = directions[axis][component];
    }
    imageIo.SetDirection(axis, direction);
  }
}
} // namespace

namespace image_io
{
WriteResult writeImage(const Image& image, const std::filesystem::path& destination, const WriteOptions& options)
{
  if (destination.empty()) {
    return failure(WriteError::EmptyPath, "No export destination was provided.");
  }
  if (!image.hasPixelData()) {
    return failure(WriteError::MissingPixelData, "The image pixel data is not loaded.");
  }
  if (options.component && *options.component >= image.header().numComponentsPerPixel()) {
    return failure(WriteError::InvalidComponent, "The selected image component does not exist.");
  }
  if (options.timePoint && *options.timePoint >= image.timeAxis().numTimePoints()) {
    return failure(WriteError::InvalidTimePoint, "The selected image time point does not exist.");
  }
  if (!options.timePoint && image.timeAxis().isTimeSeries()) {
    const std::optional<double> timeSpacing = image.timeAxis().spacing();
    if (
      !timeSpacing || !std::isfinite(*timeSpacing) || *timeSpacing <= 0.0 ||
      !std::isfinite(image.timeAxis().value(0u).value_or(std::numeric_limits<double>::quiet_NaN())))
    {
      return failure(
        WriteError::IrregularTimeAxis,
        "The image time points must be finite, regularly ordered, and positively spaced for export.");
    }
  }
  const bool standardRasterOutput = isStandardRasterImageFile(destination);
  if (
    standardRasterOutput &&
    (image.header().pixelDimensions().z != 1u || (!options.timePoint && image.timeAxis().isTimeSeries())))
  {
    return failure(
      WriteError::UnsupportedFormat,
      "Standard image files can only store one two-dimensional frame. Choose a medical image format instead.");
  }

  const itk::IOComponentEnum componentType = toItkComponentType(image.header().memoryComponentType());
  if (itk::IOComponentEnum::UNKNOWNCOMPONENTTYPE == componentType) {
    return failure(WriteError::UnsupportedComponentType, "The image's component type cannot be exported.");
  }

  const std::size_t componentBytes = image.header().memoryComponentSizeInBytes();
  if (0u == componentBytes) {
    return failure(WriteError::UnsupportedComponentType, "The image's component size is invalid.");
  }

  std::vector<std::byte> scratch;
  const void* output = nullptr;
  if (WriteResult copyResult = copySelectedPixels(image, options, componentBytes, scratch, output); !copyResult) {
    return copyResult;
  }

  try {
    const std::string destinationString = destination.string();
    itk::ImageIOBase::Pointer imageIo =
      itk::ImageIOFactory::CreateImageIO(destinationString.c_str(), itk::IOFileModeEnum::WriteMode);
    if (imageIo.IsNull()) {
      return failure(
        WriteError::UnsupportedFormat,
        "No image writer supports the destination extension '" + destination.extension().string() + "'.");
    }

    // Medical formats retain Entropy's complete 3D physical coordinate frame, including a
    // singleton third axis for planar images. Standard raster formats are necessarily 2D.
    const std::uint32_t spatialDimensions = standardRasterOutput ? 2u : 3u;
    const bool writeTimeAxis = !options.timePoint && image.timeAxis().isTimeSeries();
    const std::uint32_t outputDimensions = spatialDimensions + (writeTimeAxis ? 1u : 0u);
    const std::uint32_t outputComponents = options.component ? 1u : image.header().numComponentsPerPixel();

    imageIo->SetFileName(destinationString);
    imageIo->SetNumberOfDimensions(outputDimensions);
    imageIo->SetNumberOfComponents(outputComponents);
    imageIo->SetComponentType(componentType);
    imageIo->SetPixelType(itkPixelType(image.header().pixelType(), outputComponents));
    imageIo->SetUseCompression(options.useCompression);
    configureSpatialAxes(*imageIo, image.header(), spatialDimensions, outputDimensions);

    if (writeTimeAxis) {
      const std::uint32_t timeAxis = spatialDimensions;
      imageIo->SetDimensions(timeAxis, image.timeAxis().numTimePoints());
      imageIo->SetOrigin(timeAxis, image.timeAxis().value(0u).value_or(0.0));
      imageIo->SetSpacing(timeAxis, *image.timeAxis().spacing());
      std::vector<double> timeDirection(outputDimensions, 0.0);
      timeDirection[timeAxis] = 1.0;
      imageIo->SetDirection(timeAxis, timeDirection);
      itk::EncapsulateMetaData<std::string>(imageIo->GetMetaDataDictionary(), "entropy_time_axis", "last");
      itk::EncapsulateMetaData<std::string>(
        imageIo->GetMetaDataDictionary(),
        "entropy_time_units",
        image.timeAxis().units());
    }

    itk::ImageIORegion ioRegion(outputDimensions);
    for (std::uint32_t axis = 0u; axis < outputDimensions; ++axis) {
      ioRegion.SetIndex(axis, 0);
      ioRegion.SetSize(axis, imageIo->GetDimensions(axis));
    }
    imageIo->SetIORegion(ioRegion);

    imageIo->WriteImageInformation();
    imageIo->Write(output);
    return {};
  }
  catch (const itk::ExceptionObject& error) {
    return failure(WriteError::IoFailure, error.GetDescription());
  }
  catch (const std::exception& error) {
    return failure(WriteError::IoFailure, error.what());
  }
}
} // namespace image_io
