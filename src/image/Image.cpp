#include "image/Image.h"
#include "image/ImageCastHelper.tpp"
#include "image/ImageUtility.h"
#include "image/ImageUtility.tpp"

// clang-format off
#include <spdlog/spdlog.h>
#include <spdlog/fmt/ostr.h>
// clang-format on

#include <algorithm>
#include <array>

namespace
{
// Maximum number of components to load for images with interleaved buffer components
static constexpr uint32_t MAX_INTERLEAVED_COMPS = 4;
} // namespace

Image::Image(
  const fs::path& fileName,
  const ImageRepresentation& imageRep,
  const MultiComponentBufferType& bufferType
)
  : m_data_int8()
  , m_data_uint8()
  , m_data_int16()
  , m_data_uint16()
  , m_data_int32()
  , m_data_uint32()
  , m_data_float32()
  ,

  m_dataSorted_int8()
  , m_dataSorted_uint8()
  , m_dataSorted_int16()
  , m_dataSorted_uint16()
  , m_dataSorted_int32()
  , m_dataSorted_uint32()
  , m_dataSorted_float32()
  ,

  m_imageRep(imageRep)
  , m_bufferType(bufferType)
  ,

  m_ioInfoOnDisk()
  , m_ioInfoInMemory()
{
  const itk::ImageIOBase::Pointer imageIo = createStandardImageIo(fileName.string().c_str());

  if (!imageIo || imageIo.IsNull())
  {
    spdlog::error("Error creating itk::ImageIOBase for image from file {}", fileName);
    throw_debug("Error creating itk::ImageIOBase")
  }

  if (!m_ioInfoOnDisk.set(imageIo))
  {
    spdlog::error("Error setting image IO information for image from file {}", fileName);
    throw_debug("Error setting image IO information")
  }

  // The information in memory (destination image) may not match the information on disk (source image)
  m_ioInfoInMemory = m_ioInfoOnDisk;

  const bool isComponentFloatingPoint = m_ioInfoOnDisk.m_componentInfo.m_componentType
                                          == itk::IOComponentEnum::FLOAT
                                        || m_ioInfoOnDisk.m_componentInfo.m_componentType
                                             == itk::IOComponentEnum::DOUBLE
                                        || m_ioInfoOnDisk.m_componentInfo.m_componentType
                                             == itk::IOComponentEnum::LDOUBLE;

  // Source and destination component ITK types: Floating point images are loaded with 32-bit float
  // components and integer images are loaded with 64-bit signed integer components.
  const itk::IOComponentEnum srcItkCompType = isComponentFloatingPoint ? itk::IOComponentEnum::FLOAT
                                                                       : itk::IOComponentEnum::LONG;

  const itk::IOComponentEnum dstItkCompType = m_ioInfoInMemory.m_componentInfo.m_componentType;

  const std::size_t numPixels = m_ioInfoOnDisk.m_sizeInfo.m_imageSizeInPixels;
  const uint32_t numCompsOnDisk = m_ioInfoOnDisk.m_pixelInfo.m_numComponents;
  const bool isVectorImage = (numCompsOnDisk > 1);

  spdlog::info(
    "Attempting to open image from {} with {} pixels and {} components per pixel",
    fileName,
    numPixels,
    numCompsOnDisk
  );

  // The number of components to load in the destination image may not match the number of components in the source image.
  uint32_t numCompsToLoad = numCompsOnDisk;

  if (isVectorImage)
  {
    if (Image::MultiComponentBufferType::InterleavedImage == m_bufferType)
    {
      if (numCompsToLoad > MAX_INTERLEAVED_COMPS)
      {
        numCompsToLoad = MAX_INTERLEAVED_COMPS;
        spdlog::warn(
          "Opened image {} with {} interleaved components; only the first {} components will be "
          "loaded",
          fileName,
          numCompsOnDisk,
          numCompsToLoad
        );
      }
    }

    if (Image::ImageRepresentation::Segmentation == m_imageRep)
    {
      spdlog::warn(
        "Opened a segmentation image from {} with {} components; "
        "only the first component of the segmentation will be used",
        fileName,
        numCompsOnDisk
      );
      numCompsToLoad = 1;
    }

    // Adjust the number of components in the image header
    m_header.setNumComponentsPerPixel(numCompsToLoad);
  }

  if (0 == numCompsToLoad)
  {
    spdlog::error("No components to load for image from file {}", fileName);
    throw_debug("No components to load for image")
  }

  std::function<bool(const void* buffer, std::size_t numElements)> loadBufferFn = nullptr;

  switch (m_imageRep)
  {
  case ImageRepresentation::Image:
  {
    loadBufferFn =
      [this, &srcItkCompType, &dstItkCompType](const void* buffer, std::size_t numElements)
    { return loadImageBuffer(buffer, numElements, srcItkCompType, dstItkCompType); };
    break;
  }
  case ImageRepresentation::Segmentation:
  {
    loadBufferFn =
      [this, &srcItkCompType, &dstItkCompType](const void* buffer, std::size_t numElements)
    { return loadSegBuffer(buffer, numElements, srcItkCompType, dstItkCompType); };
    break;
  }
  }

  bool loaded = false;
  if (isComponentFloatingPoint)
  {
    // Read image with floating point components from disk to an ITK image with 32-bit float point pixel components
    loaded = loadImage<float>(
      fileName, numPixels, numCompsOnDisk, numCompsToLoad, isVectorImage, m_bufferType, loadBufferFn
    );
  }
  else
  {
    // Read image with integer components from disk to an ITK image with 64-bit signed integer pixel components
    loaded = loadImage<int64_t>(
      fileName, numPixels, numCompsOnDisk, numCompsToLoad, isVectorImage, m_bufferType, loadBufferFn
    );
  }

  if (!loaded)
  {
    throw_debug("Error loading image")
  }

  m_header = ImageHeader(
    m_ioInfoOnDisk, m_ioInfoInMemory, (MultiComponentBufferType::InterleavedImage == m_bufferType)
  );
  m_headerOverrides = ImageHeaderOverrides(
    m_header.pixelDimensions(), m_header.spacing(), m_header.origin(), m_header.directions()
  );
  m_tx = ImageTransformations(
    m_header.pixelDimensions(), m_header.spacing(), m_header.origin(), m_header.directions()
  );

  if (!generateSortedBuffers())
  {
    spdlog::error("Error generating sorted image component buffers");
    throw_debug("Error generating sorted image component buffers")
  }

  std::vector<ComponentStats> componentStats = computeImageStatistics(*this);
  m_settings = ImageSettings(
    getFileName(fileName.string(), false),
    m_header.numPixels(),
    m_header.numComponentsPerPixel(),
    m_header.memoryComponentType(),
    std::move(componentStats)
  );

  m_settings.histogramSettings();
}

Image::Image(
  const ImageHeader& header,
  const std::string& displayName,
  const ImageRepresentation& imageRep,
  const MultiComponentBufferType& bufferType,
  const std::vector<const void*>& imageDataComponents
)
  : m_data_int8()
  , m_data_uint8()
  , m_data_int16()
  , m_data_uint16()
  , m_data_int32()
  , m_data_uint32()
  , m_data_float32()
  ,

  m_dataSorted_int8()
  , m_dataSorted_uint8()
  , m_dataSorted_int16()
  , m_dataSorted_uint16()
  , m_dataSorted_int32()
  , m_dataSorted_uint32()
  , m_dataSorted_float32()
  ,

  m_imageRep(imageRep)
  , m_bufferType(bufferType)
  ,

  m_ioInfoOnDisk()
  , m_ioInfoInMemory()
  ,

  m_header(header)
{
  if (imageDataComponents.empty())
  {
    spdlog::error("No image data buffers provided for constructing Image");
    throw_debug("No image data buffers provided for constructing Image")
  }

  // The image does not exist on disk, but we need to fill this out anyway:
  m_ioInfoOnDisk.m_fileInfo.m_fileName = m_header.fileName();
  m_ioInfoOnDisk.m_componentInfo.m_componentType = toItkComponentType(m_header.memoryComponentType()
  );
  m_ioInfoOnDisk.m_componentInfo.m_componentTypeString = m_header.memoryComponentTypeAsString();

  m_ioInfoInMemory = m_ioInfoOnDisk;

  // Source and destination component ITK types
  using CType = itk::IOComponentEnum;
  const CType srcItkCompType = m_ioInfoInMemory.m_componentInfo.m_componentType;
  CType dstItkCompType = itk::IOComponentEnum::UNKNOWNCOMPONENTTYPE;

  switch (srcItkCompType)
  {
  case CType::UCHAR:
    dstItkCompType = CType::UCHAR;
    break;
  case CType::CHAR:
    dstItkCompType = CType::CHAR;
    break;
  case CType::USHORT:
    dstItkCompType = CType::USHORT;
    break;
  case CType::SHORT:
    dstItkCompType = CType::SHORT;
    break;
  case CType::UINT:
    dstItkCompType = CType::UINT;
    break;
  case CType::INT:
    dstItkCompType = CType::INT;
    break;
  case CType::FLOAT:
    dstItkCompType = CType::FLOAT;
    break;

  case CType::ULONG:
  case CType::ULONGLONG:
    dstItkCompType = CType::UINT;
    break;

  case CType::LONG:
  case CType::LONGLONG:
    dstItkCompType = CType::INT;
    break;

  case CType::DOUBLE:
  case CType::LDOUBLE:
    dstItkCompType = CType::FLOAT;
    break;

  case CType::UNKNOWNCOMPONENTTYPE:
  {
    spdlog::error(
      "Unknown component type in image from file {}", m_ioInfoOnDisk.m_fileInfo.m_fileName
    );
    throw_debug("Unknown component type in image")
  }
  }

  const std::size_t numPixels = m_header.numPixels();
  const uint32_t numComps = m_header.numComponentsPerPixel();
  const bool isVectorImage = (numComps > 1);

  if (isVectorImage)
  {
    // Create multi-component image
    uint32_t numCompsToLoad = numComps;

    if (MultiComponentBufferType::InterleavedImage == m_bufferType)
    {
      // Set a maximum of MAX_COMPS components
      numCompsToLoad = std::min(numCompsToLoad, MAX_INTERLEAVED_COMPS);

      if (numComps > MAX_INTERLEAVED_COMPS)
      {
        spdlog::warn(
          "The number of image components ({}) exceeds that maximum that will be created ({})"
          "because this image uses interleaved buffer format",
          numComps,
          MAX_INTERLEAVED_COMPS
        );
      }
    }

    if (ImageRepresentation::Segmentation == m_imageRep)
    {
      spdlog::warn("Attempting to create a segmentation image with {} components", numComps);
      spdlog::warn("Only one component of the segmentation image will be created");
      numCompsToLoad = 1;
    }

    if (0 == numCompsToLoad)
    {
      spdlog::error("No components to create for image from file {}", m_header.fileName());
      throw_debug("No components to create for image")
    }

    // Adjust the number of components in the image header
    m_header.setNumComponentsPerPixel(numCompsToLoad);

    switch (m_bufferType)
    {
    case MultiComponentBufferType::SeparateImages:
    {
      // Load each component separately:
      if (imageDataComponents.size() < m_header.numComponentsPerPixel())
      {
        spdlog::error(
          "Insufficient number of image data buffers provided: {}", imageDataComponents.size()
        );
        throw_debug("Insufficient number of image data buffers were provided")
      }

      for (std::size_t c = 0; c < m_header.numComponentsPerPixel(); ++c)
      {
        switch (m_imageRep)
        {
        case ImageRepresentation::Segmentation:
        {
          if (!loadSegBuffer(imageDataComponents[c], numPixels, srcItkCompType, dstItkCompType))
          {
            throw_debug("Error loading segmentation image buffer")
          }
          break;
        }
        case ImageRepresentation::Image:
        {
          if (!loadImageBuffer(imageDataComponents[c], numPixels, srcItkCompType, dstItkCompType))
          {
            throw_debug("Error loading image buffer")
          }
          break;
        }
        }
      }
      break;
    }
    case MultiComponentBufferType::InterleavedImage:
    {
      // Load a single buffer with interleaved components:
      const std::size_t N = numPixels * numComps;

      switch (m_imageRep)
      {
      case ImageRepresentation::Segmentation:
      {
        if (!loadSegBuffer(imageDataComponents[0], N, srcItkCompType, dstItkCompType))
        {
          throw_debug("Error loading segmentation image buffer")
        }
        break;
      }
      case ImageRepresentation::Image:
      {
        if (!loadImageBuffer(imageDataComponents[0], N, srcItkCompType, dstItkCompType))
        {
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
    if (ImageRepresentation::Segmentation == m_imageRep)
    {
      if (!loadSegBuffer(imageDataComponents[0], numPixels, srcItkCompType, dstItkCompType))
      {
        throw_debug("Error loading segmentation image buffer")
      }
    }
    else
    {
      if (!loadImageBuffer(imageDataComponents[0], numPixels, srcItkCompType, dstItkCompType))
      {
        throw_debug("Error loading image buffer")
      }
    }
  }

  m_tx = ImageTransformations(
    m_header.pixelDimensions(), m_header.spacing(), m_header.origin(), m_header.directions()
  );
  m_headerOverrides = ImageHeaderOverrides(
    m_header.pixelDimensions(), m_header.spacing(), m_header.origin(), m_header.directions()
  );

  if (!generateSortedBuffers())
  {
    spdlog::error("Error generating sorted image component buffers");
    throw_debug("Error generating sorted image component buffers")
  }

  std::vector<ComponentStats> componentStats = computeImageStatistics(*this);
  m_settings = ImageSettings(
    displayName,
    m_header.numPixels(),
    m_header.numComponentsPerPixel(),
    m_header.memoryComponentType(),
    std::move(componentStats)
  );
}

bool Image::saveComponentToDisk(uint32_t component, const std::optional<fs::path>& newFileName)
{
  constexpr uint32_t DIM = 3;
  constexpr bool s_isVectorImage = false;

  const fs::path fileName = (newFileName) ? *newFileName : m_header.fileName();

  if (component >= m_header.numComponentsPerPixel())
  {
    spdlog::error(
      "Invalid image component {} to save to disk; image has only {} components",
      component,
      m_header.numComponentsPerPixel()
    );
    return false;
  }

  std::array<uint32_t, DIM> dims;
  std::array<double, DIM> origin;
  std::array<double, DIM> spacing;
  std::array<std::array<double, DIM>, DIM> directions;

  for (uint32_t i = 0; i < DIM; ++i)
  {
    const int ii = static_cast<int>(i);
    dims[i] = m_header.pixelDimensions()[ii];
    origin[i] = static_cast<double>(m_header.origin()[ii]);
    spacing[i] = static_cast<double>(m_header.spacing()[ii]);

    directions[i] = {
      static_cast<double>(m_header.directions()[ii].x),
      static_cast<double>(m_header.directions()[ii].y),
      static_cast<double>(m_header.directions()[ii].z)
    };
  }

  switch (m_header.memoryComponentType())
  {
  case ComponentType::Int8:
  {
    auto image = makeScalarImage(dims, origin, spacing, directions, m_data_int8[component].data());
    return writeImage<int8_t, DIM, s_isVectorImage>(image, fileName);
  }
  case ComponentType::UInt8:
  {
    auto image = makeScalarImage(dims, origin, spacing, directions, m_data_uint8[component].data());
    return writeImage<uint8_t, DIM, s_isVectorImage>(image, fileName);
  }
  case ComponentType::Int16:
  {
    auto image = makeScalarImage(dims, origin, spacing, directions, m_data_int16[component].data());
    return writeImage<int16_t, DIM, s_isVectorImage>(image, fileName);
  }
  case ComponentType::UInt16:
  {
    auto image = makeScalarImage(dims, origin, spacing, directions, m_data_uint16[component].data());
    return writeImage<uint16_t, DIM, s_isVectorImage>(image, fileName);
  }
  case ComponentType::Int32:
  {
    auto image = makeScalarImage(dims, origin, spacing, directions, m_data_int32[component].data());
    return writeImage<int32_t, DIM, s_isVectorImage>(image, fileName);
  }
  case ComponentType::UInt32:
  {
    auto image = makeScalarImage(dims, origin, spacing, directions, m_data_uint32[component].data());
    return writeImage<uint32_t, DIM, s_isVectorImage>(image, fileName);
  }
  case ComponentType::Float32:
  {
    auto image = makeScalarImage(dims, origin, spacing, directions, m_data_float32[component].data());
    return writeImage<float, DIM, s_isVectorImage>(image, fileName);
  }
  default:
  {
    return false;
  }
  }
}

bool Image::generateSortedBuffers()
{
  switch (m_header.memoryComponentType())
  {
  case ComponentType::Int8:
    m_dataSorted_int8.clear();
    break;
  case ComponentType::UInt8:
    m_dataSorted_uint8.clear();
    break;
  case ComponentType::Int16:
    m_dataSorted_int16.clear();
    break;
  case ComponentType::UInt16:
    m_dataSorted_uint16.clear();
    break;
  case ComponentType::Int32:
    m_dataSorted_int32.clear();
    break;
  case ComponentType::UInt32:
    m_dataSorted_uint32.clear();
    break;
  case ComponentType::Float32:
    m_dataSorted_float32.clear();
    break;
  default:
    return false;
  }

  switch (m_bufferType)
  {
  case MultiComponentBufferType::SeparateImages:
  {
    for (std::size_t c = 0; c < m_header.numComponentsPerPixel(); ++c)
    {
      switch (m_header.memoryComponentType())
      {
      case ComponentType::Int8:
      {
        const auto& src = m_data_int8[c];
        auto& dst = m_dataSorted_int8;
        dst.emplace_back(std::vector<int8_t>(src.size()));
        std::partial_sort_copy(
          std::begin(src), std::end(src), std::begin(dst.back()), std::end(dst.back())
        );
        break;
      }
      case ComponentType::UInt8:
      {
        const auto& src = m_data_uint8[c];
        auto& dst = m_dataSorted_uint8;
        dst.emplace_back(std::vector<uint8_t>(src.size()));
        std::partial_sort_copy(
          std::begin(src), std::end(src), std::begin(dst.back()), std::end(dst.back())
        );
        break;
      }
      case ComponentType::Int16:
      {
        const auto& src = m_data_int16[c];
        auto& dst = m_dataSorted_int16;
        dst.emplace_back(std::vector<int16_t>(src.size()));
        std::partial_sort_copy(
          std::begin(src), std::end(src), std::begin(dst.back()), std::end(dst.back())
        );
        break;
      }
      case ComponentType::UInt16:
      {
        const auto& src = m_data_uint16[c];
        auto& dst = m_dataSorted_uint16;
        dst.emplace_back(std::vector<uint16_t>(src.size()));
        std::partial_sort_copy(
          std::begin(src), std::end(src), std::begin(dst.back()), std::end(dst.back())
        );
        break;
      }
      case ComponentType::Int32:
      {
        const auto& src = m_data_int32[c];
        auto& dst = m_dataSorted_int32;
        dst.emplace_back(std::vector<int32_t>(src.size()));
        std::partial_sort_copy(
          std::begin(src), std::end(src), std::begin(dst.back()), std::end(dst.back())
        );
        break;
      }
      case ComponentType::UInt32:
      {
        const auto& src = m_data_uint32[c];
        auto& dst = m_dataSorted_uint32;
        dst.emplace_back(std::vector<uint32_t>(src.size()));
        std::partial_sort_copy(
          std::begin(src), std::end(src), std::begin(dst.back()), std::end(dst.back())
        );
        break;
      }
      case ComponentType::Float32:
      {
        const auto& src = m_data_float32[c];
        auto& dst = m_dataSorted_float32;
        dst.emplace_back(std::vector<float>(src.size()));
        std::partial_sort_copy(
          std::begin(src), std::end(src), std::begin(dst.back()), std::end(dst.back())
        );
        break;
      }
      default:
        return false;
      }
    }
    return true;
  }
  case MultiComponentBufferType::InterleavedImage:
  {
    const std::size_t N = m_header.numPixels();
    for (std::size_t c = 0; c < m_header.numComponentsPerPixel(); ++c)
    {
      switch (m_header.memoryComponentType())
      {
      case ComponentType::Int8:
      {
        auto& dst = m_dataSorted_int8[c];
        dst.resize(N);
        for (std::size_t i = 0; i < N; ++i)
        {
          dst[i] = m_data_int8[0][(c + 1ul) * i];
        }
        std::sort(std::begin(dst), std::end(dst));
        break;
      }
      case ComponentType::UInt8:
      {
        auto& dst = m_dataSorted_uint8[c];
        dst.resize(N);
        for (std::size_t i = 0; i < N; ++i)
        {
          dst[i] = m_data_uint8[0][(c + 1ul) * i];
        }
        std::sort(std::begin(dst), std::end(dst));
        break;
      }
      case ComponentType::Int16:
      {
        auto& dst = m_dataSorted_int16[c];
        dst.resize(N);
        for (std::size_t i = 0; i < N; ++i)
        {
          dst[i] = m_data_int16[0][(c + 1ul) * i];
        }
        std::sort(std::begin(dst), std::end(dst));
        break;
      }
      case ComponentType::UInt16:
      {
        auto& dst = m_dataSorted_uint16[c];
        dst.resize(N);
        for (std::size_t i = 0; i < N; ++i)
        {
          dst[i] = m_data_uint16[0][(c + 1ul) * i];
        }
        std::sort(std::begin(dst), std::end(dst));
        break;
      }
      case ComponentType::Int32:
      {
        auto& dst = m_dataSorted_int32[c];
        dst.resize(N);
        for (std::size_t i = 0; i < N; ++i)
        {
          dst[i] = m_data_int32[0][(c + 1ul) * i];
        }
        std::sort(std::begin(dst), std::end(dst));
        break;
      }
      case ComponentType::UInt32:
      {
        auto& dst = m_dataSorted_uint32[c];
        dst.resize(N);
        for (std::size_t i = 0; i < N; ++i)
        {
          dst[i] = m_data_uint32[0][(c + 1ul) * i];
        }
        std::sort(std::begin(dst), std::end(dst));
        break;
      }
      case ComponentType::Float32:
      {
        auto& dst = m_dataSorted_float32[c];
        dst.resize(N);
        for (std::size_t i = 0; i < N; ++i)
        {
          dst[i] = m_data_float32[0][(c + 1ul) * i];
        }
        std::sort(std::begin(dst), std::end(dst));
        break;
      }
      default:
        return false;
      }
    }
    return true;
  }
  }

  return false;
}

bool Image::loadImageBuffer(
  const void* buffer,
  std::size_t numElements,
  const itk::IOComponentEnum& srcComponentType,
  const itk::IOComponentEnum& dstComponentType
)
{
  using CType = itk::ImageIOBase::IOComponentType;

  bool didCast = false;
  bool warnSizeConversion = false;

  switch (dstComponentType)
  {
  case CType::UCHAR:
  {
    m_data_uint8.emplace_back(createBuffer<uint8_t>(buffer, numElements, srcComponentType));
    break;
  }
  case CType::CHAR:
  {
    m_data_int8.emplace_back(createBuffer<int8_t>(buffer, numElements, srcComponentType));
    break;
  }
  case CType::USHORT:
  {
    m_data_uint16.emplace_back(createBuffer<uint16_t>(buffer, numElements, srcComponentType));
    break;
  }
  case CType::SHORT:
  {
    m_data_int16.emplace_back(createBuffer<int16_t>(buffer, numElements, srcComponentType));
    break;
  }
  case CType::UINT:
  {
    m_data_uint32.emplace_back(createBuffer<uint32_t>(buffer, numElements, srcComponentType));
    break;
  }
  case CType::INT:
  {
    m_data_int32.emplace_back(createBuffer<int32_t>(buffer, numElements, srcComponentType));
    break;
  }
  case CType::FLOAT:
  {
    m_data_float32.emplace_back(createBuffer<float>(buffer, numElements, srcComponentType));
    break;
  }

  case CType::ULONG:
  case CType::ULONGLONG:
  {
    m_data_uint32.emplace_back(createBuffer<uint32_t>(buffer, numElements, srcComponentType));
    m_ioInfoInMemory.m_componentInfo.m_componentType = CType::UINT;
    m_ioInfoInMemory.m_componentInfo.m_componentSizeInBytes = 4;
    didCast = true;
    warnSizeConversion = true;
    break;
  }

  case CType::LONG:
  case CType::LONGLONG:
  {
    m_data_int32.emplace_back(createBuffer<int32_t>(buffer, numElements, srcComponentType));
    m_ioInfoInMemory.m_componentInfo.m_componentType = CType::INT;
    m_ioInfoInMemory.m_componentInfo.m_componentSizeInBytes = 4;
    didCast = true;
    warnSizeConversion = true;
    break;
  }

  case CType::DOUBLE:
  case CType::LDOUBLE:
  {
    m_data_float32.emplace_back(createBuffer<float>(buffer, numElements, srcComponentType));
    m_ioInfoInMemory.m_componentInfo.m_componentType = CType::FLOAT;
    m_ioInfoInMemory.m_componentInfo.m_componentSizeInBytes = 4;
    didCast = true;
    warnSizeConversion = true;
    break;
  }

  case CType::UNKNOWNCOMPONENTTYPE:
  {
    spdlog::error(
      "Unknown component type in image from file {}", m_ioInfoOnDisk.m_fileInfo.m_fileName
    );
    return false;
  }
  }

  if (didCast)
  {
    const std::string newTypeString = itk::ImageIOBase::GetComponentTypeAsString(
      m_ioInfoInMemory.m_componentInfo.m_componentType
    );

    m_ioInfoInMemory.m_componentInfo.m_componentTypeString = newTypeString;

    m_ioInfoInMemory.m_sizeInfo.m_imageSizeInBytes
      = numElements * m_ioInfoInMemory.m_componentInfo.m_componentSizeInBytes;

    spdlog::info(
      "Casted image pixel component from type {} to {}",
      m_ioInfoOnDisk.m_componentInfo.m_componentTypeString,
      newTypeString
    );

    if (warnSizeConversion)
    {
      spdlog::warn(
        "Size conversion: Possible loss of information when casting image pixel "
        "component from type {} to {}",
        m_ioInfoOnDisk.m_componentInfo.m_componentTypeString,
        newTypeString
      );
    }
  }

  return true;
}

bool Image::loadSegBuffer(
  const void* buffer,
  std::size_t numElements,
  const itk::IOComponentEnum& srcComponentType,
  const itk::IOComponentEnum& dstComponentType
)
{
  using CType = itk::ImageIOBase::IOComponentType;

  bool didCast = false;
  bool warnFloatConversion = false;
  bool warnSizeConversion = false;
  bool warnSignConversion = false;

  switch (dstComponentType)
  {
  // No casting is needed for the cases of unsigned integers with 8, 16, or 32 bytes:
  case CType::UCHAR:
  {
    m_data_uint8.emplace_back(createBuffer<uint8_t>(buffer, numElements, srcComponentType));
    break;
  }
  case CType::USHORT:
  {
    m_data_uint16.emplace_back(createBuffer<uint16_t>(buffer, numElements, srcComponentType));
    break;
  }
  case CType::UINT:
  {
    m_data_uint32.emplace_back(createBuffer<uint32_t>(buffer, numElements, srcComponentType));
    break;
  }

  // Signed 8-, 16-, and 32-bit integers are cast to unsigned 8-, 16-, and 32-bit integers:
  case CType::CHAR:
  {
    m_data_uint8.emplace_back(createBuffer<uint8_t>(buffer, numElements, srcComponentType));
    m_ioInfoInMemory.m_componentInfo.m_componentType = CType::UCHAR;
    m_ioInfoInMemory.m_componentInfo.m_componentSizeInBytes = 1;
    didCast = true;
    warnSignConversion = true;
    break;
  }
  case CType::SHORT:
  {
    m_data_uint16.emplace_back(createBuffer<uint16_t>(buffer, numElements, srcComponentType));
    m_ioInfoInMemory.m_componentInfo.m_componentType = CType::USHORT;
    m_ioInfoInMemory.m_componentInfo.m_componentSizeInBytes = 2;
    didCast = true;
    warnSignConversion = true;
    break;
  }
  case CType::INT:
  {
    m_data_uint32.emplace_back(createBuffer<uint32_t>(buffer, numElements, srcComponentType));
    m_ioInfoInMemory.m_componentInfo.m_componentType = CType::UINT;
    m_ioInfoInMemory.m_componentInfo.m_componentSizeInBytes = 4;
    didCast = true;
    warnSignConversion = true;
    break;
  }

  // Unsigned long (64-bit) and long long (128-bit) integers are cast to unsigned 32-bit integers:
  case CType::ULONG:
  case CType::ULONGLONG:
  {
    m_data_uint32.emplace_back(createBuffer<uint32_t>(buffer, numElements, srcComponentType));
    m_ioInfoInMemory.m_componentInfo.m_componentType = CType::UINT;
    m_ioInfoInMemory.m_componentInfo.m_componentSizeInBytes = 4;
    didCast = true;
    warnSizeConversion = true;
    break;
  }

  // Signed long (64-bit) and long long (128-bit) integers are cast to unsigned 32-bit integers:
  case CType::LONG:
  case CType::LONGLONG:
  {
    m_data_uint32.emplace_back(createBuffer<uint32_t>(buffer, numElements, srcComponentType));
    m_ioInfoInMemory.m_componentInfo.m_componentType = CType::UINT;
    m_ioInfoInMemory.m_componentInfo.m_componentSizeInBytes = 4;
    didCast = true;
    warnSizeConversion = true;
    warnSignConversion = true;
    break;
  }

  // Floating-points are cast to unsigned 32-bit integers:
  case CType::FLOAT:
  case CType::DOUBLE:
  case CType::LDOUBLE:
  {
    m_data_uint32.emplace_back(createBuffer<uint32_t>(buffer, numElements, srcComponentType));
    m_ioInfoInMemory.m_componentInfo.m_componentType = CType::UINT;
    m_ioInfoInMemory.m_componentInfo.m_componentSizeInBytes = 4;
    didCast = true;
    warnFloatConversion = true;
    warnSignConversion = true;
    break;
  }

  case CType::UNKNOWNCOMPONENTTYPE:
  {
    spdlog::error(
      "Unknown component type in image from file {}", m_ioInfoOnDisk.m_fileInfo.m_fileName
    );
    return false;
  }
  }

  if (didCast)
  {
    const std::string newTypeString = itk::ImageIOBase::GetComponentTypeAsString(
      m_ioInfoInMemory.m_componentInfo.m_componentType
    );

    m_ioInfoInMemory.m_componentInfo.m_componentTypeString = newTypeString;
    m_ioInfoInMemory.m_sizeInfo.m_imageSizeInBytes
      = numElements * m_ioInfoInMemory.m_componentInfo.m_componentSizeInBytes;

    spdlog::info(
      "Casted segmentation {} pixel component from type {} to {}",
      m_ioInfoOnDisk.m_fileInfo.m_fileName,
      m_ioInfoOnDisk.m_componentInfo.m_componentTypeString,
      newTypeString
    );

    if (warnFloatConversion)
    {
      spdlog::warn(
        "Floating point to integer conversion: Possible loss of precision and information when "
        "casting "
        "segmentation pixel component from type {} to {}",
        m_ioInfoOnDisk.m_componentInfo.m_componentTypeString,
        newTypeString
      );
    }

    if (warnSizeConversion)
    {
      spdlog::warn(
        "Size conversion: Possible loss of information when casting segmentation pixel component "
        "from type "
        "{} to {}",
        m_ioInfoOnDisk.m_componentInfo.m_componentTypeString,
        newTypeString
      );
    }

    if (warnSignConversion)
    {
      spdlog::warn(
        "Signed to unsigned integer conversion: Possible loss of information when casting "
        "segmentation pixel "
        "component from type {} to {}",
        m_ioInfoOnDisk.m_componentInfo.m_componentTypeString,
        newTypeString
      );
    }
  }

  return true;
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

const void* Image::bufferAsVoid(uint32_t comp) const
{
  auto F = [this](uint32_t i) -> const void*
  {
    switch (m_header.memoryComponentType())
    {
    case ComponentType::Int8:
      return static_cast<const void*>(m_data_int8.at(i).data());
    case ComponentType::UInt8:
      return static_cast<const void*>(m_data_uint8.at(i).data());
    case ComponentType::Int16:
      return static_cast<const void*>(m_data_int16.at(i).data());
    case ComponentType::UInt16:
      return static_cast<const void*>(m_data_uint16.at(i).data());
    case ComponentType::Int32:
      return static_cast<const void*>(m_data_int32.at(i).data());
    case ComponentType::UInt32:
      return static_cast<const void*>(m_data_uint32.at(i).data());
    case ComponentType::Float32:
      return static_cast<const void*>(m_data_float32.at(i).data());
    default:
      return static_cast<const void*>(nullptr);
    }
  };

  switch (m_bufferType)
  {
  case MultiComponentBufferType::SeparateImages:
  {
    if (m_header.numComponentsPerPixel() <= comp)
    {
      return nullptr;
    }
    return F(comp);
  }
  case MultiComponentBufferType::InterleavedImage:
  {
    if (1 <= comp)
    {
      return nullptr;
    }
    return F(0);
  }
  }

  return nullptr;
}

void* Image::bufferAsVoid(uint32_t comp)
{
  return const_cast<void*>(const_cast<const Image*>(this)->bufferAsVoid(comp));
}

const void* Image::bufferSortedAsVoid(uint32_t comp) const
{
  if (m_header.numComponentsPerPixel() <= comp)
  {
    spdlog::error(
      "Invalid image component {} when retrieving sorted buffer for image with {} components",
      comp,
      m_header.numComponentsPerPixel()
    );
    return nullptr;
  }

  switch (m_header.memoryComponentType())
  {
  case ComponentType::Int8:
    return static_cast<const void*>(m_dataSorted_int8.at(comp).data());
  case ComponentType::UInt8:
    return static_cast<const void*>(m_dataSorted_uint8.at(comp).data());
  case ComponentType::Int16:
    return static_cast<const void*>(m_dataSorted_int16.at(comp).data());
  case ComponentType::UInt16:
    return static_cast<const void*>(m_dataSorted_uint16.at(comp).data());
  case ComponentType::Int32:
    return static_cast<const void*>(m_dataSorted_int32.at(comp).data());
  case ComponentType::UInt32:
    return static_cast<const void*>(m_dataSorted_uint32.at(comp).data());
  case ComponentType::Float32:
    return static_cast<const void*>(m_dataSorted_float32.at(comp).data());
  default:
    return static_cast<const void*>(nullptr);
  }
}

void* Image::bufferSortedAsVoid(uint32_t comp)
{
  return const_cast<void*>(const_cast<const Image*>(this)->bufferSortedAsVoid(comp));
}

std::optional<std::pair<std::size_t, std::size_t>> Image::getComponentAndOffsetForBuffer(
  uint32_t comp, int i, int j, int k
) const
{
  const glm::u64vec3 dims = m_header.pixelDimensions();

  const std::size_t index = dims.x * dims.y * static_cast<std::size_t>(k)
                            + dims.x * static_cast<std::size_t>(j) + static_cast<std::size_t>(i);

  return getComponentAndOffsetForBuffer(comp, index);
}

std::optional<std::pair<std::size_t, std::size_t>> Image::getComponentAndOffsetForBuffer(
  uint32_t comp, std::size_t index
) const
{
  if (comp > m_header.numComponentsPerPixel())
  {
    spdlog::error("Invalid image component {} (image has {})", comp, m_header.numComponentsPerPixel());
    return std::nullopt;
  }

  switch (m_bufferType)
  {
  case MultiComponentBufferType::SeparateImages:
  {
    return std::make_pair(comp, index);
  }
  case MultiComponentBufferType::InterleavedImage:
  {
    // There is just one buffer (0) that holds all components
    return std::make_pair(0, static_cast<std::size_t>(comp + 1) * index);
  }
  }

  return std::nullopt;
}

QuantileOfValue Image::valueToQuantile(uint32_t comp, int64_t value) const
{
  if (comp > m_header.numComponentsPerPixel())
  {
    spdlog::error(
      "Invalid image component {} (image has {}) when converting value {} to quantile",
      comp,
      m_header.numComponentsPerPixel(),
      value
    );
    throw_debug("Invalid image component")
  }

  switch (m_header.memoryComponentType())
  {
  case ComponentType::Int8:
  {
    return convertValueToQuantile<
      int8_t>(std::span{m_dataSorted_int8[comp]}, static_cast<int8_t>(value));
  }
  case ComponentType::UInt8:
  {
    return convertValueToQuantile<
      uint8_t>(std::span{m_dataSorted_uint8[comp]}, static_cast<uint8_t>(value));
  }
  case ComponentType::Int16:
  {
    return convertValueToQuantile<
      int16_t>(std::span{m_dataSorted_int16[comp]}, static_cast<int16_t>(value));
  }
  case ComponentType::UInt16:
  {
    return convertValueToQuantile<
      uint16_t>(std::span{m_dataSorted_uint16[comp]}, static_cast<uint16_t>(value));
  }
  case ComponentType::Int32:
  {
    return convertValueToQuantile<
      int32_t>(std::span{m_dataSorted_int32[comp]}, static_cast<int32_t>(value));
  }
  case ComponentType::UInt32:
  {
    return convertValueToQuantile<
      uint32_t>(std::span{m_dataSorted_uint32[comp]}, static_cast<uint32_t>(value));
  }
  case ComponentType::Float32:
  {
    return convertValueToQuantile<
      float>(std::span{m_dataSorted_float32[comp]}, static_cast<float>(value));
  }
  default:
  {
    spdlog::error("Invalid memory component type '{}'", m_header.memoryComponentTypeAsString());
    throw_debug("Invalid memory component type")
  }
  }
}

QuantileOfValue Image::valueToQuantile(uint32_t comp, double value) const
{
  if (comp > m_header.numComponentsPerPixel())
  {
    spdlog::error(
      "Invalid image component {} (image has {}) when converting value {} to quantile",
      comp,
      m_header.numComponentsPerPixel(),
      value
    );
    throw_debug("Invalid image component")
  }

  switch (m_header.memoryComponentType())
  {
  case ComponentType::Int8:
  {
    return convertValueToQuantile(std::span{m_dataSorted_int8[comp]}, static_cast<int8_t>(value));
  }
  case ComponentType::UInt8:
  {
    return convertValueToQuantile(std::span{m_dataSorted_uint8[comp]}, static_cast<uint8_t>(value));
  }
  case ComponentType::Int16:
  {
    return convertValueToQuantile(std::span{m_dataSorted_int16[comp]}, static_cast<int16_t>(value));
  }
  case ComponentType::UInt16:
  {
    return convertValueToQuantile(std::span{m_dataSorted_uint16[comp]}, static_cast<uint16_t>(value));
  }
  case ComponentType::Int32:
  {
    return convertValueToQuantile(std::span{m_dataSorted_int32[comp]}, static_cast<int32_t>(value));
  }
  case ComponentType::UInt32:
  {
    return convertValueToQuantile(std::span{m_dataSorted_uint32[comp]}, static_cast<uint32_t>(value));
  }
  case ComponentType::Float32:
  {
    return convertValueToQuantile(std::span{m_dataSorted_float32[comp]}, static_cast<float>(value));
  }
  default:
  {
    spdlog::error("Invalid memory component type '{}'", m_header.memoryComponentTypeAsString());
    throw_debug("Invalid memory component type")
  }
  }
}

double Image::quantileToValue(uint32_t comp, double quantile) const
{
  if (comp > m_header.numComponentsPerPixel())
  {
    spdlog::error(
      "Invalid image component {} (image has {}) when converting quantile {} to value",
      comp,
      m_header.numComponentsPerPixel(),
      quantile
    );
    throw_debug("Invalid image component")
  }

  switch (m_header.memoryComponentType())
  {
  case ComponentType::Int8:
  {
    return static_cast<double>(convertQuantileToValue(std::span{m_dataSorted_int8[comp]}, quantile));
  }
  case ComponentType::UInt8:
  {
    return static_cast<double>(convertQuantileToValue(std::span{m_dataSorted_uint8[comp]}, quantile)
    );
  }
  case ComponentType::Int16:
  {
    return static_cast<double>(convertQuantileToValue(std::span{m_dataSorted_int16[comp]}, quantile)
    );
  }
  case ComponentType::UInt16:
  {
    return static_cast<double>(convertQuantileToValue(std::span{m_dataSorted_uint16[comp]}, quantile)
    );
  }
  case ComponentType::Int32:
  {
    return static_cast<double>(convertQuantileToValue(std::span{m_dataSorted_int32[comp]}, quantile)
    );
  }
  case ComponentType::UInt32:
  {
    return static_cast<double>(convertQuantileToValue(std::span{m_dataSorted_uint32[comp]}, quantile)
    );
  }
  case ComponentType::Float32:
  {
    return static_cast<double>(
      convertQuantileToValue(std::span{m_dataSorted_float32[comp]}, quantile)
    );
  }
  default:
  {
    spdlog::error("Invalid memory component type '{}'", m_header.memoryComponentTypeAsString());
    throw_debug("Invalid memory component type")
  }
  }
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
  for (const auto& p : m_ioInfoInMemory.m_metaData)
  {
    os << p.first << ": ";
    // std::visit([&os] (const auto& e) { os << e; }, p.second);
    os << std::endl;
  }
  return os;
}

void Image::updateComponentStats()
{
  /// @todo regenerate sorted buffers?
  m_settings.updateWithNewComponentStatistics(computeImageStatistics(*this), false);
}
