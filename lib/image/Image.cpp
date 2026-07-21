#include "image/Image.h"
#include "internal/ImageCastHelper.tpp"
#include "image/ImageWindowDefaults.h"
#include "image/ImageUtility.h"
#include "internal/ImageUtilityItk.h"
#include "internal/ImageUtility.tpp"

// clang-format off
#include <spdlog/spdlog.h>
#include <spdlog/fmt/std.h>
#include <spdlog/fmt/ostr.h>
// clang-format on

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstring>
#include <limits>
#include <optional>
#include <utility>
#include <vector>

namespace fs = std::filesystem;

namespace
{
std::size_t componentSizeInBytes(ComponentType componentType)
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
    case ComponentType::Long:
    case ComponentType::ULong:
    case ComponentType::LongLong:
    case ComponentType::ULongLong:
    case ComponentType::Float64:
      return 8;
    case ComponentType::LongDouble:
      return sizeof(long double);
    case ComponentType::Undefined:
      return 0;
  }

  return 0;
}

std::vector<std::byte> compactInterleavedComponents(
  const void* buffer,
  std::size_t numPixels,
  uint32_t sourceComponents,
  uint32_t componentsToKeep,
  ComponentType componentType)
{
  const std::size_t componentBytes = componentSizeInBytes(componentType);
  std::vector<std::byte> compacted(numPixels * componentsToKeep * componentBytes);

  if (!buffer || componentBytes == 0 || componentsToKeep >= sourceComponents) {
    return compacted;
  }

  const auto* src = static_cast<const std::byte*>(buffer);
  for (std::size_t i = 0; i < numPixels; ++i) {
    for (uint32_t c = 0; c < componentsToKeep; ++c) {
      const std::size_t srcOffset = (i * sourceComponents + c) * componentBytes;
      const std::size_t dstOffset = (i * componentsToKeep + c) * componentBytes;
      std::memcpy(compacted.data() + dstOffset, src + srcOffset, componentBytes);
    }
  }

  return compacted;
}

void setDefaultComponentRendering(
  ImageSettings& settings,
  const ImageHeader& header,
  Image::ImageRepresentation imageRep,
  const std::vector<ComponentStats>& componentStats)
{
  if (Image::ImageRepresentation::Image == imageRep && header.numComponentsPerPixel() > 1u) {
    const bool standardRasterColorImage = isStandardRasterColorImage(header);
    settings.setComponentRenderMode(
      standardRasterColorImage ? ComponentRenderMode::Color : ComponentRenderMode::Magnitude);
    if (standardRasterColorImage && !componentStats.empty()) {
      // RGB/RGBA raster images are display data. Unsigned integer color channels should open with
      // their full channel range, while float or signed color data uses the measured data range.
      const std::optional<image_window_defaults::WindowRange> range =
        image_window_defaults::rasterColorComponentWindow(header.memoryComponentType(), componentStats.front());
      if (range) {
        settings.setWindowRangeForAllComponents({range->low, range->high});
      }
    }
    if (standardRasterColorImage && PixelType::RGBA == header.pixelType()) {
      settings.setIgnoreAlpha(true);
    }
  }
}

float defaultVectorGridMillimeterSpacing(const ImageHeader& header)
{
  const glm::vec3& spacing = header.spacing();
  float minSpacing = std::numeric_limits<float>::max();
  for (int axis = 0; axis < 3; ++axis) {
    if (spacing[axis] > 0.0f) {
      minSpacing = std::min(minSpacing, spacing[axis]);
    }
  }
  return 4.0f * (minSpacing == std::numeric_limits<float>::max() ? 1.0f : minSpacing);
}

void setDefaultVectorFieldRendering(ImageSettings& settings, const ImageHeader& header)
{
  if (
    (PixelType::Vector == header.pixelType() || PixelType::CovariantVector == header.pixelType() ||
     PixelType::VariableLengthVector == header.pixelType()) &&
    3u == header.numComponentsPerPixel() && ComponentType::Float32 == header.memoryComponentType())
  {
    settings.setVectorWarpedGridMillimeterSpacing(defaultVectorGridMillimeterSpacing(header));
  }
}

bool isIntegerComponentType(ComponentType componentType)
{
  switch (componentType) {
    case ComponentType::Int8:
    case ComponentType::UInt8:
    case ComponentType::Int16:
    case ComponentType::UInt16:
    case ComponentType::Int32:
    case ComponentType::UInt32:
      return true;
    case ComponentType::Float32:
    case ComponentType::Float64:
    case ComponentType::LongDouble:
    case ComponentType::Long:
    case ComponentType::ULong:
    case ComponentType::LongLong:
    case ComponentType::ULongLong:
    case ComponentType::Undefined:
      return false;
  }

  return false;
}

bool hasLabelLikeIntegerValues(const Image& image)
{
  if (!isIntegerComponentType(image.header().memoryComponentType()) || 0u == image.header().numPixels()) {
    return false;
  }

  constexpr std::size_t k_maxSamples = 100000;
  constexpr std::size_t k_maxLabelValues = 64;
  constexpr double k_maxUniqueFraction = 0.02;

  const std::size_t numPixels = image.header().numPixels();
  const std::size_t sampleCount = std::min(numPixels, k_maxSamples);
  const std::size_t stride = std::max<std::size_t>(1, numPixels / sampleCount);

  for (uint32_t component = 0; component < image.header().numComponentsPerPixel(); ++component) {
    std::vector<double> uniqueValues;
    uniqueValues.reserve(k_maxLabelValues + 1u);

    for (std::size_t pixel = 0, samples = 0; pixel < numPixels && samples < sampleCount; pixel += stride, ++samples) {
      const std::optional<double> value = image.value<double>(component, pixel);
      if (!value) {
        continue;
      }
      if (std::find(uniqueValues.begin(), uniqueValues.end(), *value) == uniqueValues.end()) {
        uniqueValues.push_back(*value);
        if (uniqueValues.size() > k_maxLabelValues) {
          return false;
        }
      }
    }

    const double uniqueFraction =
      sampleCount > 0u ? static_cast<double>(uniqueValues.size()) / static_cast<double>(sampleCount) : 1.0;
    if (uniqueValues.size() > k_maxLabelValues || uniqueFraction > k_maxUniqueFraction) {
      return false;
    }
  }

  return true;
}

InterpolationMode defaultInterpolationMode(const ImageRepresentation& imageRep, bool labelLikeIntegerValues)
{
  if (ImageRepresentation::Segmentation == imageRep || labelLikeIntegerValues) {
    return InterpolationMode::NearestNeighbor;
  }

  return InterpolationMode::Linear;
}

void setDefaultInterpolationModes(
  ImageSettings& settings,
  const ImageRepresentation& imageRep,
  bool labelLikeIntegerValues = false)
{
  const InterpolationMode mode = defaultInterpolationMode(imageRep, labelLikeIntegerValues);
  for (uint32_t component = 0; component < settings.numComponents(); ++component) {
    settings.setInterpolationMode(component, mode);
  }
  settings.setColorInterpolationMode(mode);
}

ImageTimeAxis timeAxisFromIoInfo(const ImageIoInfo& info)
{
  return {
    info.m_timeInfo.m_numTimePoints,
    info.m_timeInfo.m_origin,
    info.m_timeInfo.m_spacing,
    info.m_timeInfo.m_units};
}
} // namespace

Image::Image(const fs::path& fileName, const ImageRepresentation& imageRep, const MultiComponentBufferType& bufferType)
  : m_imageRep(imageRep), m_bufferType(bufferType)
{
  const itk::ImageIOBase::Pointer imageIo = createStandardImageIo(fileName.string().c_str());

  if (!imageIo || imageIo.IsNull()) {
    spdlog::error("Error creating itk::ImageIOBase for image from file {}", fileName);
    throwDebug("Error creating itk::ImageIOBase");
  }

  if (!setImageIoInfoFromItk(m_ioInfoOnDisk, imageIo)) {
    spdlog::error("Error setting image IO information for image from file {}", fileName);
    throwDebug("Error setting image IO information");
  }
  normalizeImageIoAxesForEntropy(m_ioInfoOnDisk, fileName);

  // The in-memory image model is spatially 3D. A 4D source is represented as 3D frames plus a
  // separate time axis.
  m_ioInfoInMemory = spatializedImageIoInfoForEntropy(m_ioInfoOnDisk);
  m_timeAxis = timeAxisFromIoInfo(m_ioInfoOnDisk);

  const bool componentIsFloatingPoint = isComponentFloatingPoint(m_ioInfoOnDisk.m_componentInfo.m_componentType);

  // Source and destination component types: Floating point images are loaded with 32-bit float
  // components and integer images are loaded with 64-bit signed integer components.
  const ComponentType srcCompType = componentIsFloatingPoint ? ComponentType::Float32 : ComponentType::LongLong;

  const ComponentType dstCompType = m_ioInfoInMemory.m_componentInfo.m_componentType;

  const std::size_t numPixels = m_ioInfoOnDisk.m_sizeInfo.m_imageSizeInPixels;
  const uint32_t numCompsOnDisk = m_ioInfoOnDisk.m_pixelInfo.m_numComponents;
  const bool isMultiComponentImage = (numCompsOnDisk > 1);

  spdlog::info(
    "Attempting to open image from {} with {} pixels and {} components per pixel",
    fileName,
    numPixels,
    numCompsOnDisk);

  // The number of components to load in the destination image may not match the number of
  // components in the source image. Segmentations are always loaded as scalar label images.
  const uint32_t componentsToLoad = componentCountToLoad(m_imageRep, numCompsOnDisk);

  if (isMultiComponentImage && Image::ImageRepresentation::Segmentation == m_imageRep) {
    spdlog::warn(
      "Opened a segmentation image from {} with {} components; "
      "only the first component of the segmentation will be used",
      fileName,
      numCompsOnDisk);
  }

  if (0 == componentsToLoad) {
    spdlog::error("No components to load for image from file {}", fileName);
    throwDebug("No components to load for image");
  }

  m_ioInfoInMemory.m_pixelInfo.m_numComponents = componentsToLoad;
  m_ioInfoInMemory.m_pixelInfo.m_pixelStrideInBytes =
    componentsToLoad * m_ioInfoInMemory.m_componentInfo.m_componentSizeInBytes;

  std::function<bool(const void* buffer, std::size_t numElements)> loadBufferFn = nullptr;

  switch (m_imageRep) {
    case ImageRepresentation::Image: {
      loadBufferFn = [this, &srcCompType, &dstCompType](const void* buffer, std::size_t numElements) {
        return loadImageBuffer(buffer, numElements, srcCompType, dstCompType);
      };
      break;
    }
    case ImageRepresentation::Segmentation: {
      loadBufferFn = [this, &srcCompType, &dstCompType](const void* buffer, std::size_t numElements) {
        return loadSegBuffer(buffer, numElements, srcCompType, dstCompType);
      };
      break;
    }
  }

  bool loaded = false;
  if (componentIsFloatingPoint) {
    // Read image with floating point components from disk to an ITK image with 32-bit float point
    // pixel components
    loaded = loadImage<float>(
      fileName,
      m_ioInfoOnDisk.m_spaceInfo.m_numDimensions,
      numPixels,
      componentsToLoad,
      isMultiComponentImage,
      m_bufferType,
      loadBufferFn);
  }
  else {
    // Read image with integer components from disk to an ITK image with 64-bit signed integer pixel
    // components
    loaded = loadImage<int64_t>(
      fileName,
      m_ioInfoOnDisk.m_spaceInfo.m_numDimensions,
      numPixels,
      componentsToLoad,
      isMultiComponentImage,
      m_bufferType,
      loadBufferFn);
  }

  if (!loaded) {
    throwDebug("Error loading image");
  }

  m_header =
    ImageHeader(m_ioInfoOnDisk, m_ioInfoInMemory, (MultiComponentBufferType::InterleavedImage == m_bufferType));
  m_headerOverrides =
    ImageHeaderOverrides(m_header.pixelDimensions(), m_header.spacing(), m_header.origin(), m_header.directions());
  m_tx = ImageTransformations(m_header.pixelDimensions(), m_header.spacing(), m_header.origin(), m_header.directions());

  std::vector<OnlineStats> onlineStats = computeImageStatisticsOnUnsortedValues(*this);
  m_tdigests = computeTDigests(*this);

  std::vector<ComponentStats> componentStats(onlineStats.size());

  for (std::size_t i = 0; i < componentStats.size(); ++i) {
    componentStats[i].onlineStats = onlineStats[i];

    for (unsigned int q = 0; q <= 100; ++q) {
      componentStats[i].quantiles[q] = m_tdigests[i].quantile(q / 100.0);
    }
  }

  m_settings = ImageSettings(
    getFileName(fileName.string(), false),
    m_header.numPixels(),
    m_header.numComponentsPerPixel(),
    m_header.memoryComponentType(),
    componentStats);
  setDefaultComponentRendering(m_settings, m_header, m_imageRep, componentStats);
  setDefaultVectorFieldRendering(m_settings, m_header);
  setDefaultInterpolationModes(m_settings, m_imageRep, hasLabelLikeIntegerValues(*this));

  m_loadState = LoadState::LoadedPixels;
}

Image::Image(
  const ImageHeader& header,
  const std::string& displayName,
  const ImageRepresentation& imageRep,
  const MultiComponentBufferType& bufferType)
  : m_imageRep(imageRep)
  , m_bufferType(bufferType)
  , m_header(header)
  , m_headerOverrides(header.pixelDimensions(), header.spacing(), header.origin(), header.directions())
  , m_tx(header.pixelDimensions(), header.spacing(), header.origin(), header.directions())
  , m_loadState(LoadState::HeaderOnly)
{
  std::vector<ComponentStats> componentStats(m_header.numComponentsPerPixel());
  for (auto& stats : componentStats) {
    stats.onlineStats.count = m_header.numPixels();
    stats.quantiles.fill(0.0L);
  }

  m_settings = ImageSettings(
    displayName,
    m_header.numPixels(),
    m_header.numComponentsPerPixel(),
    m_header.memoryComponentType(),
    componentStats);
  setDefaultComponentRendering(m_settings, m_header, m_imageRep, componentStats);
  setDefaultVectorFieldRendering(m_settings, m_header);
  setDefaultInterpolationModes(m_settings, m_imageRep, hasLabelLikeIntegerValues(*this));
}

Image::Image(
  ImageHeader header,
  const std::string& displayName,
  const ImageRepresentation& imageRep,
  const MultiComponentBufferType& bufferType,
  const std::vector<const void*>& imageDataComponents,
  ImageTimeAxis timeAxis)
  : m_imageRep(imageRep), m_bufferType(bufferType), m_timeAxis(std::move(timeAxis)), m_header(std::move(header))
{
  if (imageDataComponents.empty()) {
    spdlog::error("No image data buffers provided for constructing Image");
    throwDebug("No image data buffers provided for constructing Image");
  }

  // The image does not exist on disk, but we need to fill this out anyway:
  m_ioInfoOnDisk.m_fileInfo.m_fileName = m_header.fileName();
  m_ioInfoOnDisk.m_componentInfo.m_componentType = m_header.memoryComponentType();
  m_ioInfoOnDisk.m_componentInfo.m_componentTypeString = m_header.memoryComponentTypeAsString();
  m_ioInfoInMemory = m_ioInfoOnDisk;

  // Source and destination component types
  using CType = ComponentType;
  const CType srcCompType = m_ioInfoInMemory.m_componentInfo.m_componentType;
  CType dstCompType = ComponentType::Undefined;

  switch (srcCompType) {
    case CType::UInt8: {
      dstCompType = CType::UInt8;
      break;
    }
    case CType::Int8: {
      dstCompType = CType::Int8;
      break;
    }
    case CType::UInt16: {
      dstCompType = CType::UInt16;
      break;
    }
    case CType::Int16: {
      dstCompType = CType::Int16;
      break;
    }
    case CType::UInt32: {
      dstCompType = CType::UInt32;
      break;
    }
    case CType::Int32: {
      dstCompType = CType::Int32;
      break;
    }
    case CType::Float32: {
      dstCompType = CType::Float32;
      break;
    }

    case CType::ULong:
    case CType::ULongLong: {
      dstCompType = CType::UInt32;
      break;
    }
    case CType::Long:
    case CType::LongLong: {
      dstCompType = CType::Int32;
      break;
    }
    case CType::Float64:
    case CType::LongDouble: {
      dstCompType = CType::Float32;
      break;
    }

    case CType::Undefined: {
      spdlog::error("Unknown component type in image from file {}", m_ioInfoOnDisk.m_fileInfo.m_fileName);
      throwDebug("Unknown component type in image");
    }
  }

  const std::size_t numPixels = m_header.numPixels();
  const std::size_t numBufferPixels = numPixels * m_timeAxis.numTimePoints();
  const uint32_t numComps = m_header.numComponentsPerPixel();
  const bool isMultiComponentImage = (numComps > 1);

  if (isMultiComponentImage) {
    // Create a multi-component image. Segmentations are scalar label images, so only the first
    // component is retained when raw segmentation input has multiple components.
    const uint32_t componentsToLoad = componentCountToLoad(m_imageRep, numComps);

    if (componentsToLoad < numComps) {
      spdlog::warn("Attempting to create a segmentation image with {} components", numComps);
      spdlog::warn("Only one component of the segmentation image will be created");
    }

    if (0 == componentsToLoad) {
      spdlog::error("No components to create for image from file {}", m_header.fileName());
      throwDebug("No components to create for image");
    }

    // Adjust the number of components in the image header
    m_header.setNumComponentsPerPixel(componentsToLoad);

    switch (m_bufferType) {
      case MultiComponentBufferType::SeparateImages: {
        // Load each component separately:
        if (imageDataComponents.size() < m_header.numComponentsPerPixel()) {
          spdlog::error("Insufficient number of image data buffers provided: {}", imageDataComponents.size());
          throwDebug("Insufficient number of image data buffers were provided");
        }

        for (std::size_t c = 0; c < m_header.numComponentsPerPixel(); ++c) {
          switch (m_imageRep) {
            case ImageRepresentation::Segmentation: {
              if (!loadSegBuffer(imageDataComponents[c], numBufferPixels, srcCompType, dstCompType)) {
                throwDebug("Error loading segmentation image buffer");
              }
              break;
            }
            case ImageRepresentation::Image: {
              if (!loadImageBuffer(imageDataComponents[c], numBufferPixels, srcCompType, dstCompType)) {
                throwDebug("Error loading image buffer");
              }
              break;
            }
          }
        }
        break;
      }
      case MultiComponentBufferType::InterleavedImage: {
        // Load a single buffer with interleaved components:
        const std::size_t N = numBufferPixels * componentsToLoad;
        const void* buffer = imageDataComponents[0];
        std::vector<std::byte> compactedBuffer;

        if (componentsToLoad < numComps) {
          compactedBuffer =
            compactInterleavedComponents(buffer, numBufferPixels, numComps, componentsToLoad, srcCompType);
          buffer = compactedBuffer.data();
        }

        switch (m_imageRep) {
          case ImageRepresentation::Segmentation: {
            if (!loadSegBuffer(buffer, N, srcCompType, dstCompType)) {
              throwDebug("Error loading segmentation image buffer");
            }
            break;
          }
          case ImageRepresentation::Image: {
            if (!loadImageBuffer(buffer, N, srcCompType, dstCompType)) {
              throwDebug("Error loading image buffer");
            }
            break;
          }
        }
      }
    }
  }
  else // scalar image
  {
    if (ImageRepresentation::Segmentation == m_imageRep) {
      if (!loadSegBuffer(imageDataComponents[0], numBufferPixels, srcCompType, dstCompType)) {
        throwDebug("Error loading segmentation image buffer");
      }
    }
    else {
      if (!loadImageBuffer(imageDataComponents[0], numBufferPixels, srcCompType, dstCompType)) {
        throwDebug("Error loading image buffer");
      }
    }
  }

  m_tx = ImageTransformations(m_header.pixelDimensions(), m_header.spacing(), m_header.origin(), m_header.directions());
  m_headerOverrides =
    ImageHeaderOverrides(m_header.pixelDimensions(), m_header.spacing(), m_header.origin(), m_header.directions());

  std::vector<OnlineStats> onlineStats = computeImageStatisticsOnUnsortedValues(*this);
  m_tdigests = computeTDigests(*this);

  std::vector<ComponentStats> componentStats(onlineStats.size());

  for (std::size_t i = 0; i < componentStats.size(); ++i) {
    componentStats[i].onlineStats = onlineStats[i];

    for (unsigned int q = 0; q <= 100; ++q) {
      componentStats[i].quantiles[q] = m_tdigests[i].quantile(q / 100.0);
    }
  }

  m_settings = ImageSettings(
    displayName,
    m_header.numPixels(),
    m_header.numComponentsPerPixel(),
    m_header.memoryComponentType(),
    componentStats);
  setDefaultComponentRendering(m_settings, m_header, m_imageRep, componentStats);
  setDefaultVectorFieldRendering(m_settings, m_header);
  setDefaultInterpolationModes(m_settings, m_imageRep, hasLabelLikeIntegerValues(*this));

  m_loadState = LoadState::LoadedPixels;
}

Image::LoadState Image::loadState() const
{
  return m_loadState;
}

void Image::setLoadState(LoadState state)
{
  m_loadState = state;
}

bool Image::hasPixelData() const
{
  return LoadState::LoadedPixels == m_loadState;
}

const Image::ImageRepresentation& Image::imageRep() const
{
  return m_imageRep;
}

const Image::MultiComponentBufferType& Image::bufferType() const
{
  return m_bufferType;
}

const ImageHeader& Image::header() const
{
  return m_header;
}

ImageHeader& Image::header()
{
  return m_header;
}

bool Image::isTimeSeries() const
{
  return m_timeAxis.isTimeSeries();
}

const ImageTimeAxis& Image::timeAxis() const
{
  return m_timeAxis;
}

const ImageTransformations& Image::transformations() const
{
  return m_tx;
}

ImageTransformations& Image::transformations()
{
  return m_tx;
}

const ImageSettings& Image::settings() const
{
  return m_settings;
}

ImageSettings& Image::settings()
{
  return m_settings;
}

void Image::setUseIdentityPixelSpacings(bool identitySpacings)
{
  m_headerOverrides.m_useIdentityPixelSpacings = identitySpacings;
  m_header.setHeaderOverrides(m_headerOverrides);
  m_tx.setHeaderOverrides(m_headerOverrides);
}

bool Image::getUseIdentityPixelSpacings() const
{
  return m_headerOverrides.m_useIdentityPixelSpacings;
}

void Image::setUseZeroPixelOrigin(bool zeroOrigin)
{
  m_headerOverrides.m_useZeroPixelOrigin = zeroOrigin;
  m_header.setHeaderOverrides(m_headerOverrides);
  m_tx.setHeaderOverrides(m_headerOverrides);
}

bool Image::getUseZeroPixelOrigin() const
{
  return m_headerOverrides.m_useZeroPixelOrigin;
}

void Image::setUseIdentityPixelDirections(bool useIdentity)
{
  m_headerOverrides.m_useIdentityPixelDirections = useIdentity;
  m_header.setHeaderOverrides(m_headerOverrides);
  m_tx.setHeaderOverrides(m_headerOverrides);
}

bool Image::getUseIdentityPixelDirections() const
{
  return m_headerOverrides.m_useIdentityPixelDirections;
}

void Image::setSnapToClosestOrthogonalPixelDirections(bool snap)
{
  m_headerOverrides.m_snapToClosestOrthogonalPixelDirections = snap;
  m_header.setHeaderOverrides(m_headerOverrides);
  m_tx.setHeaderOverrides(m_headerOverrides);
}

bool Image::getSnapToClosestOrthogonalPixelDirections() const
{
  return m_headerOverrides.m_snapToClosestOrthogonalPixelDirections;
}

void Image::setHeaderOverrides(const ImageHeaderOverrides& overrides)
{
  m_headerOverrides = overrides;
  m_header.setHeaderOverrides(m_headerOverrides);
  m_tx.setHeaderOverrides(m_headerOverrides);
}

const ImageHeaderOverrides& Image::getHeaderOverrides() const
{
  return m_headerOverrides;
}

void Image::setUserSpatialMetadata(const ImageSpatialMetadata& metadata)
{
  m_header.setUserSpatialMetadata(metadata);
  m_tx.setImageGeometry(m_header.pixelDimensions(), m_header.spacing(), m_header.origin(), m_header.directions());
}

std::ostream& Image::metaData(std::ostream& os) const
{
  for (const auto& p : m_ioInfoInMemory.m_metaData) {
    os << p.first << ": ";
    // std::visit([&os] (const auto& e) { os << e; }, p.second);
    os << '\n';
  }
  return os;
}

void Image::updateComponentStats()
{
  std::vector<OnlineStats> onlineStats = computeImageStatisticsOnUnsortedValues(*this);
  m_tdigests = computeTDigests(*this);

  std::vector<ComponentStats> componentStats(onlineStats.size());

  for (std::size_t i = 0; i < componentStats.size(); ++i) {
    componentStats[i].onlineStats = onlineStats[i];

    for (unsigned int q = 0; q <= 100; ++q) {
      componentStats[i].quantiles[q] = m_tdigests[i].quantile(q / 100.0);
    }
  }

  m_settings.updateWithNewComponentStatistics(std::move(componentStats), false);
}

/// @todo Put this back when using sorted buffers for stats
// if (!generateSortedBuffers()) {
//   spdlog::error("Error generating sorted image component buffers");
//   throwDebug("Error generating sorted image component buffers")
// }
// m_settings.setUsingExactQuantiles(true);
// std::vector<ComponentStats> componentStats = computeImageStatisticsOnSortedValues(*this);
