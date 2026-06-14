#include "image/Image.h"
#include "internal/ImageCastHelper.tpp"
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
#include <vector>

namespace fs = std::filesystem;

namespace
{
// Maximum number of components to load for images with interleaved buffer components
constexpr uint32_t MAX_INTERLEAVED_COMPS = 4;

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
} // namespace

Image::Image(const fs::path& fileName, const ImageRepresentation& imageRep, const MultiComponentBufferType& bufferType)
  : m_imageRep(imageRep), m_bufferType(bufferType)
{
  const itk::ImageIOBase::Pointer imageIo = createStandardImageIo(fileName.string().c_str());

  if (!imageIo || imageIo.IsNull()) {
    spdlog::error("Error creating itk::ImageIOBase for image from file {}", fileName);
    throw_debug("Error creating itk::ImageIOBase")
  }

  /// @todo Handle 4D images
  if (!setImageIoInfoFromItk(m_ioInfoOnDisk, imageIo)) {
    spdlog::error("Error setting image IO information for image from file {}", fileName);
    throw_debug("Error setting image IO information")
  }

  // The information in memory (destination image) may not match the information on disk (source
  // image)
  m_ioInfoInMemory = m_ioInfoOnDisk;

  const bool componentIsFloatingPoint = isComponentFloatingPoint(m_ioInfoOnDisk.m_componentInfo.m_componentType);

  // Source and destination component types: Floating point images are loaded with 32-bit float
  // components and integer images are loaded with 64-bit signed integer components.
  const ComponentType srcCompType = componentIsFloatingPoint ? ComponentType::Float32 : ComponentType::LongLong;

  const ComponentType dstCompType = m_ioInfoInMemory.m_componentInfo.m_componentType;

  const std::size_t numPixels = m_ioInfoOnDisk.m_sizeInfo.m_imageSizeInPixels;
  const uint32_t numCompsOnDisk = m_ioInfoOnDisk.m_pixelInfo.m_numComponents;
  const bool isVectorImage = (numCompsOnDisk > 1);

  spdlog::info(
    "Attempting to open image from {} with {} pixels and {} components per pixel",
    fileName,
    numPixels,
    numCompsOnDisk);

  // The number of components to load in the destination image may not match the number of
  // components in the source image.
  uint32_t numCompsToLoad = numCompsOnDisk;

  if (isVectorImage) {
    if (Image::MultiComponentBufferType::InterleavedImage == m_bufferType) {
      if (numCompsToLoad > MAX_INTERLEAVED_COMPS) {
        numCompsToLoad = MAX_INTERLEAVED_COMPS;
        spdlog::warn(
          "Opened image {} with {} interleaved components; only the first {} components will be "
          "loaded",
          fileName,
          numCompsOnDisk,
          numCompsToLoad);
      }
    }

    if (Image::ImageRepresentation::Segmentation == m_imageRep) {
      spdlog::warn(
        "Opened a segmentation image from {} with {} components; "
        "only the first component of the segmentation will be used",
        fileName,
        numCompsOnDisk);
      numCompsToLoad = 1;
    }

    // Adjust the number of components in the image header
    m_header.setNumComponentsPerPixel(numCompsToLoad);
  }

  if (0 == numCompsToLoad) {
    spdlog::error("No components to load for image from file {}", fileName);
    throw_debug("No components to load for image")
  }

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
    loaded =
      loadImage<float>(fileName, numPixels, numCompsOnDisk, numCompsToLoad, isVectorImage, m_bufferType, loadBufferFn);
  }
  else {
    // Read image with integer components from disk to an ITK image with 64-bit signed integer pixel
    // components
    loaded = loadImage<int64_t>(
      fileName,
      numPixels,
      numCompsOnDisk,
      numCompsToLoad,
      isVectorImage,
      m_bufferType,
      loadBufferFn);
  }

  if (!loaded) {
    throw_debug("Error loading image")
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

  switch (m_imageRep) {
    case ImageRepresentation::Image: {
      m_settings.setInterpolationMode(InterpolationMode::Linear);
      break;
    }
    case ImageRepresentation::Segmentation: {
      m_settings.setInterpolationMode(InterpolationMode::NearestNeighbor);
      break;
    }
  }

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
    std::move(componentStats));

  switch (m_imageRep) {
    case ImageRepresentation::Image: {
      m_settings.setInterpolationMode(InterpolationMode::Linear);
      break;
    }
    case ImageRepresentation::Segmentation: {
      m_settings.setInterpolationMode(InterpolationMode::NearestNeighbor);
      break;
    }
  }
}

Image::Image(
  const ImageHeader& header,
  const std::string& displayName,
  const ImageRepresentation& imageRep,
  const MultiComponentBufferType& bufferType,
  const std::vector<const void*>& imageDataComponents)
  : m_imageRep(imageRep), m_bufferType(bufferType), m_header(header)
{
  if (imageDataComponents.empty()) {
    spdlog::error("No image data buffers provided for constructing Image");
    throw_debug("No image data buffers provided for constructing Image")
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
      throw_debug("Unknown component type in image")
    }
  }

  const std::size_t numPixels = m_header.numPixels();
  const uint32_t numComps = m_header.numComponentsPerPixel();
  const bool isVectorImage = (numComps > 1);

  if (isVectorImage) {
    // Create multi-component image
    uint32_t numCompsToLoad = numComps;

    if (MultiComponentBufferType::InterleavedImage == m_bufferType) {
      // Set a maximum of MAX_COMPS components
      numCompsToLoad = std::min(numCompsToLoad, MAX_INTERLEAVED_COMPS);

      if (numComps > MAX_INTERLEAVED_COMPS) {
        spdlog::warn(
          "The number of image components ({}) exceeds that maximum that will be created ({})"
          "because this image uses interleaved buffer format",
          numComps,
          MAX_INTERLEAVED_COMPS);
      }
    }

    if (ImageRepresentation::Segmentation == m_imageRep) {
      spdlog::warn("Attempting to create a segmentation image with {} components", numComps);
      spdlog::warn("Only one component of the segmentation image will be created");
      numCompsToLoad = 1;
    }

    if (0 == numCompsToLoad) {
      spdlog::error("No components to create for image from file {}", m_header.fileName());
      throw_debug("No components to create for image")
    }

    // Adjust the number of components in the image header
    m_header.setNumComponentsPerPixel(numCompsToLoad);

    switch (m_bufferType) {
      case MultiComponentBufferType::SeparateImages: {
        // Load each component separately:
        if (imageDataComponents.size() < m_header.numComponentsPerPixel()) {
          spdlog::error("Insufficient number of image data buffers provided: {}", imageDataComponents.size());
          throw_debug("Insufficient number of image data buffers were provided")
        }

        for (std::size_t c = 0; c < m_header.numComponentsPerPixel(); ++c) {
          switch (m_imageRep) {
            case ImageRepresentation::Segmentation: {
              if (!loadSegBuffer(imageDataComponents[c], numPixels, srcCompType, dstCompType)) {
                throw_debug("Error loading segmentation image buffer")
              }
              break;
            }
            case ImageRepresentation::Image: {
              if (!loadImageBuffer(imageDataComponents[c], numPixels, srcCompType, dstCompType)) {
                throw_debug("Error loading image buffer")
              }
              break;
            }
          }
        }
        break;
      }
      case MultiComponentBufferType::InterleavedImage: {
        // Load a single buffer with interleaved components:
        const std::size_t N = numPixels * numCompsToLoad;
        const void* buffer = imageDataComponents[0];
        std::vector<std::byte> compactedBuffer;

        if (numCompsToLoad < numComps) {
          compactedBuffer = compactInterleavedComponents(buffer, numPixels, numComps, numCompsToLoad, srcCompType);
          buffer = compactedBuffer.data();
        }

        switch (m_imageRep) {
          case ImageRepresentation::Segmentation: {
            if (!loadSegBuffer(buffer, N, srcCompType, dstCompType)) {
              throw_debug("Error loading segmentation image buffer")
            }
            break;
          }
          case ImageRepresentation::Image: {
            if (!loadImageBuffer(buffer, N, srcCompType, dstCompType)) {
              throw_debug("Error loading image buffer")
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
      if (!loadSegBuffer(imageDataComponents[0], numPixels, srcCompType, dstCompType)) {
        throw_debug("Error loading segmentation image buffer")
      }
    }
    else {
      if (!loadImageBuffer(imageDataComponents[0], numPixels, srcCompType, dstCompType)) {
        throw_debug("Error loading image buffer")
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
    std::move(componentStats));

  switch (m_imageRep) {
    case ImageRepresentation::Image: {
      m_settings.setInterpolationMode(InterpolationMode::Linear);
      break;
    }
    case ImageRepresentation::Segmentation: {
      m_settings.setInterpolationMode(InterpolationMode::NearestNeighbor);
      break;
    }
  }

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

std::ostream& Image::metaData(std::ostream& os) const
{
  for (const auto& p : m_ioInfoInMemory.m_metaData) {
    os << p.first << ": ";
    // std::visit([&os] (const auto& e) { os << e; }, p.second);
    os << std::endl;
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
//   throw_debug("Error generating sorted image component buffers")
// }
// m_settings.setUsingExactQuantiles(true);
// std::vector<ComponentStats> componentStats = computeImageStatisticsOnSortedValues(*this);
