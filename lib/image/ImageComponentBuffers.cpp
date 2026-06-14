#include "image/Image.h"

#include "internal/ImageCastHelper.tpp"
#include "internal/ImageUtility.tpp"
#include "internal/ImageUtilityItk.h"
#include "image/ImageUtility.h"

#include <spdlog/fmt/std.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <array>
#include <vector>

namespace fs = std::filesystem;

namespace
{
template<typename T>
std::vector<T> copyComponentValues(const Image& image, uint32_t component)
{
  std::vector<T> values;
  values.reserve(image.header().numPixels());

  for (std::size_t i = 0; i < image.header().numPixels(); ++i) {
    const auto value = image.value<T>(component, i);
    if (!value) {
      return {};
    }
    values.push_back(*value);
  }

  return values;
}

template<typename T>
const T* componentBufferForSave(const Image& image, uint32_t component, std::vector<T>& scratch)
{
  if (Image::MultiComponentBufferType::InterleavedImage == image.bufferType()) {
    scratch = copyComponentValues<T>(image, component);
    return scratch.size() == image.header().numPixels() ? scratch.data() : nullptr;
  }

  return static_cast<const T*>(image.bufferAsVoid(component));
}
} // namespace

bool Image::saveComponentToDisk(uint32_t component, const std::optional<fs::path>& newFileName)
{
  constexpr uint32_t DIM = 3;
  constexpr bool isVectorImage = false;
  const fs::path fileName = (newFileName) ? *newFileName : m_header.fileName();

  if (component >= m_header.numComponentsPerPixel()) {
    spdlog::error(
      "Invalid image component {} to save to disk; the image has {} components",
      component,
      m_header.numComponentsPerPixel());
    return false;
  }

  if (!hasPixelData()) {
    spdlog::error("Cannot save image component {} to disk; pixel data is not loaded", component);
    return false;
  }

  std::array<uint32_t, DIM> dims;
  std::array<double, DIM> origin;
  std::array<double, DIM> spacing;
  std::array<std::array<double, DIM>, DIM> directions;

  for (uint32_t i = 0; i < DIM; ++i) {
    const int ii = static_cast<int>(i);
    dims[i] = m_header.pixelDimensions()[ii];
    origin[i] = static_cast<double>(m_header.origin()[ii]);
    spacing[i] = static_cast<double>(m_header.spacing()[ii]);
    directions[i] = {
      static_cast<double>(m_header.directions()[ii].x),
      static_cast<double>(m_header.directions()[ii].y),
      static_cast<double>(m_header.directions()[ii].z)};
  }

  switch (m_header.memoryComponentType()) {
    case ComponentType::Int8: {
      std::vector<int8_t> scratch;
      const auto* buffer = componentBufferForSave<int8_t>(*this, component, scratch);
      if (!buffer) {
        return false;
      }
      auto image = makeScalarImage(dims, origin, spacing, directions, buffer);
      return writeImage<int8_t, DIM, isVectorImage>(image, fileName);
    }
    case ComponentType::UInt8: {
      std::vector<uint8_t> scratch;
      const auto* buffer = componentBufferForSave<uint8_t>(*this, component, scratch);
      if (!buffer) {
        return false;
      }
      auto image = makeScalarImage(dims, origin, spacing, directions, buffer);
      return writeImage<uint8_t, DIM, isVectorImage>(image, fileName);
    }
    case ComponentType::Int16: {
      std::vector<int16_t> scratch;
      const auto* buffer = componentBufferForSave<int16_t>(*this, component, scratch);
      if (!buffer) {
        return false;
      }
      auto image = makeScalarImage(dims, origin, spacing, directions, buffer);
      return writeImage<int16_t, DIM, isVectorImage>(image, fileName);
    }
    case ComponentType::UInt16: {
      std::vector<uint16_t> scratch;
      const auto* buffer = componentBufferForSave<uint16_t>(*this, component, scratch);
      if (!buffer) {
        return false;
      }
      auto image = makeScalarImage(dims, origin, spacing, directions, buffer);
      return writeImage<uint16_t, DIM, isVectorImage>(image, fileName);
    }
    case ComponentType::Int32: {
      std::vector<int32_t> scratch;
      const auto* buffer = componentBufferForSave<int32_t>(*this, component, scratch);
      if (!buffer) {
        return false;
      }
      auto image = makeScalarImage(dims, origin, spacing, directions, buffer);
      return writeImage<int32_t, DIM, isVectorImage>(image, fileName);
    }
    case ComponentType::UInt32: {
      std::vector<uint32_t> scratch;
      const auto* buffer = componentBufferForSave<uint32_t>(*this, component, scratch);
      if (!buffer) {
        return false;
      }
      auto image = makeScalarImage(dims, origin, spacing, directions, buffer);
      return writeImage<uint32_t, DIM, isVectorImage>(image, fileName);
    }
    case ComponentType::Float32: {
      std::vector<float> scratch;
      const auto* buffer = componentBufferForSave<float>(*this, component, scratch);
      if (!buffer) {
        return false;
      }
      auto image = makeScalarImage(dims, origin, spacing, directions, buffer);
      return writeImage<float, DIM, isVectorImage>(image, fileName);
    }
    default: {
      return false;
    }
  }
}

bool Image::generateSortedBuffers()
{
  switch (m_header.memoryComponentType()) {
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

  switch (m_bufferType) {
    case MultiComponentBufferType::SeparateImages: {
      for (std::size_t c = 0; c < m_header.numComponentsPerPixel(); ++c) {
        switch (m_header.memoryComponentType()) {
          case ComponentType::Int8: {
            const auto& src = m_data_int8[c];
            auto& dst = m_dataSorted_int8;
            dst.emplace_back(src);
            std::sort(dst.back().begin(), dst.back().end());
            break;
          }
          case ComponentType::UInt8: {
            const auto& src = m_data_uint8[c];
            auto& dst = m_dataSorted_uint8;
            dst.emplace_back(src);
            std::sort(dst.back().begin(), dst.back().end());
            break;
          }
          case ComponentType::Int16: {
            const auto& src = m_data_int16[c];
            auto& dst = m_dataSorted_int16;
            dst.emplace_back(src);
            std::sort(dst.back().begin(), dst.back().end());
            break;
          }
          case ComponentType::UInt16: {
            const auto& src = m_data_uint16[c];
            auto& dst = m_dataSorted_uint16;
            dst.emplace_back(src);
            std::sort(dst.back().begin(), dst.back().end());
            break;
          }
          case ComponentType::Int32: {
            const auto& src = m_data_int32[c];
            auto& dst = m_dataSorted_int32;
            dst.emplace_back(src);
            std::sort(dst.back().begin(), dst.back().end());
            break;
          }
          case ComponentType::UInt32: {
            const auto& src = m_data_uint32[c];
            auto& dst = m_dataSorted_uint32;
            dst.emplace_back(src);
            std::sort(dst.back().begin(), dst.back().end());
            break;
          }
          case ComponentType::Float32: {
            const auto& src = m_data_float32[c];
            auto& dst = m_dataSorted_float32;
            dst.emplace_back(src);
            std::sort(dst.back().begin(), dst.back().end());
            break;
          }
          default:
            return false;
        }
      }
      return true;
    }
    case MultiComponentBufferType::InterleavedImage: {
      const std::size_t N = m_header.numPixels();
      const std::size_t numComponents = m_header.numComponentsPerPixel();

      switch (m_header.memoryComponentType()) {
        case ComponentType::Int8:
          m_dataSorted_int8.resize(numComponents);
          break;
        case ComponentType::UInt8:
          m_dataSorted_uint8.resize(numComponents);
          break;
        case ComponentType::Int16:
          m_dataSorted_int16.resize(numComponents);
          break;
        case ComponentType::UInt16:
          m_dataSorted_uint16.resize(numComponents);
          break;
        case ComponentType::Int32:
          m_dataSorted_int32.resize(numComponents);
          break;
        case ComponentType::UInt32:
          m_dataSorted_uint32.resize(numComponents);
          break;
        case ComponentType::Float32:
          m_dataSorted_float32.resize(numComponents);
          break;
        default:
          return false;
      }

      for (std::size_t c = 0; c < m_header.numComponentsPerPixel(); ++c) {
        switch (m_header.memoryComponentType()) {
          case ComponentType::Int8: {
            auto& dst = m_dataSorted_int8[c];
            dst.resize(N);
            for (std::size_t i = 0; i < N; ++i) {
              dst[i] = m_data_int8[0][numComponents * i + c];
            }
            std::sort(std::begin(dst), std::end(dst));
            break;
          }
          case ComponentType::UInt8: {
            auto& dst = m_dataSorted_uint8[c];
            dst.resize(N);
            for (std::size_t i = 0; i < N; ++i) {
              dst[i] = m_data_uint8[0][numComponents * i + c];
            }
            std::sort(std::begin(dst), std::end(dst));
            break;
          }
          case ComponentType::Int16: {
            auto& dst = m_dataSorted_int16[c];
            dst.resize(N);
            for (std::size_t i = 0; i < N; ++i) {
              dst[i] = m_data_int16[0][numComponents * i + c];
            }
            std::sort(std::begin(dst), std::end(dst));
            break;
          }
          case ComponentType::UInt16: {
            auto& dst = m_dataSorted_uint16[c];
            dst.resize(N);
            for (std::size_t i = 0; i < N; ++i) {
              dst[i] = m_data_uint16[0][numComponents * i + c];
            }
            std::sort(std::begin(dst), std::end(dst));
            break;
          }
          case ComponentType::Int32: {
            auto& dst = m_dataSorted_int32[c];
            dst.resize(N);
            for (std::size_t i = 0; i < N; ++i) {
              dst[i] = m_data_int32[0][numComponents * i + c];
            }
            std::sort(std::begin(dst), std::end(dst));
            break;
          }
          case ComponentType::UInt32: {
            auto& dst = m_dataSorted_uint32[c];
            dst.resize(N);
            for (std::size_t i = 0; i < N; ++i) {
              dst[i] = m_data_uint32[0][numComponents * i + c];
            }
            std::sort(std::begin(dst), std::end(dst));
            break;
          }
          case ComponentType::Float32: {
            auto& dst = m_dataSorted_float32[c];
            dst.resize(N);
            for (std::size_t i = 0; i < N; ++i) {
              dst[i] = m_data_float32[0][numComponents * i + c];
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
  ComponentType srcComponentType,
  ComponentType dstComponentType)
{
  using CType = ComponentType;

  bool didCast = false;
  bool warnSizeConversion = false;

  switch (dstComponentType) {
    case CType::UInt8:
      m_data_uint8.emplace_back(createBuffer<uint8_t>(buffer, numElements, srcComponentType));
      break;
    case CType::Int8:
      m_data_int8.emplace_back(createBuffer<int8_t>(buffer, numElements, srcComponentType));
      break;
    case CType::UInt16:
      m_data_uint16.emplace_back(createBuffer<uint16_t>(buffer, numElements, srcComponentType));
      break;
    case CType::Int16:
      m_data_int16.emplace_back(createBuffer<int16_t>(buffer, numElements, srcComponentType));
      break;
    case CType::UInt32:
      m_data_uint32.emplace_back(createBuffer<uint32_t>(buffer, numElements, srcComponentType));
      break;
    case CType::Int32:
      m_data_int32.emplace_back(createBuffer<int32_t>(buffer, numElements, srcComponentType));
      break;
    case CType::Float32:
      m_data_float32.emplace_back(createBuffer<float>(buffer, numElements, srcComponentType));
      break;
    case CType::ULong:
    case CType::ULongLong:
      m_data_uint32.emplace_back(createBuffer<uint32_t>(buffer, numElements, srcComponentType));
      m_ioInfoInMemory.m_componentInfo.m_componentType = CType::UInt32;
      m_ioInfoInMemory.m_componentInfo.m_componentSizeInBytes = 4;
      didCast = true;
      warnSizeConversion = true;
      break;
    case CType::Long:
    case CType::LongLong:
      m_data_int32.emplace_back(createBuffer<int32_t>(buffer, numElements, srcComponentType));
      m_ioInfoInMemory.m_componentInfo.m_componentType = CType::Int32;
      m_ioInfoInMemory.m_componentInfo.m_componentSizeInBytes = 4;
      didCast = true;
      warnSizeConversion = true;
      break;
    case CType::Float64:
    case CType::LongDouble:
      m_data_float32.emplace_back(createBuffer<float>(buffer, numElements, srcComponentType));
      m_ioInfoInMemory.m_componentInfo.m_componentType = CType::Float32;
      m_ioInfoInMemory.m_componentInfo.m_componentSizeInBytes = 4;
      didCast = true;
      warnSizeConversion = true;
      break;
    case CType::Undefined:
      spdlog::error("Unknown component type in image from file {}", m_ioInfoOnDisk.m_fileInfo.m_fileName);
      return false;
  }

  if (didCast) {
    const std::string newTypeString = componentTypeString(m_ioInfoInMemory.m_componentInfo.m_componentType);
    m_ioInfoInMemory.m_componentInfo.m_componentTypeString = newTypeString;
    m_ioInfoInMemory.m_sizeInfo.m_imageSizeInBytes =
      numElements * m_ioInfoInMemory.m_componentInfo.m_componentSizeInBytes;

    spdlog::info(
      "Casted image pixel component from type {} to {}",
      m_ioInfoOnDisk.m_componentInfo.m_componentTypeString,
      newTypeString);

    if (warnSizeConversion) {
      spdlog::warn(
        "Size conversion: Possible loss of information when casting image pixel "
        "component from type {} to {}",
        m_ioInfoOnDisk.m_componentInfo.m_componentTypeString,
        newTypeString);
    }
  }

  return true;
}

bool Image::loadSegBuffer(
  const void* buffer,
  std::size_t numElements,
  ComponentType srcComponentType,
  ComponentType dstComponentType)
{
  using CType = ComponentType;

  bool didCast = false;
  bool warnFloatConversion = false;
  bool warnSizeConversion = false;
  bool warnSignConversion = false;

  switch (dstComponentType) {
    case CType::UInt8:
      m_data_uint8.emplace_back(createBuffer<uint8_t>(buffer, numElements, srcComponentType));
      break;
    case CType::UInt16:
      m_data_uint16.emplace_back(createBuffer<uint16_t>(buffer, numElements, srcComponentType));
      break;
    case CType::UInt32:
      m_data_uint32.emplace_back(createBuffer<uint32_t>(buffer, numElements, srcComponentType));
      break;
    case CType::Int8:
      m_data_uint8.emplace_back(createBuffer<uint8_t>(buffer, numElements, srcComponentType));
      m_ioInfoInMemory.m_componentInfo.m_componentType = CType::UInt8;
      m_ioInfoInMemory.m_componentInfo.m_componentSizeInBytes = 1;
      didCast = true;
      warnSignConversion = true;
      break;
    case CType::Int16:
      m_data_uint16.emplace_back(createBuffer<uint16_t>(buffer, numElements, srcComponentType));
      m_ioInfoInMemory.m_componentInfo.m_componentType = CType::UInt16;
      m_ioInfoInMemory.m_componentInfo.m_componentSizeInBytes = 2;
      didCast = true;
      warnSignConversion = true;
      break;
    case CType::Int32:
      m_data_uint32.emplace_back(createBuffer<uint32_t>(buffer, numElements, srcComponentType));
      m_ioInfoInMemory.m_componentInfo.m_componentType = CType::UInt32;
      m_ioInfoInMemory.m_componentInfo.m_componentSizeInBytes = 4;
      didCast = true;
      warnSignConversion = true;
      break;
    case CType::ULong:
    case CType::ULongLong:
      m_data_uint32.emplace_back(createBuffer<uint32_t>(buffer, numElements, srcComponentType));
      m_ioInfoInMemory.m_componentInfo.m_componentType = CType::UInt32;
      m_ioInfoInMemory.m_componentInfo.m_componentSizeInBytes = 4;
      didCast = true;
      warnSizeConversion = true;
      break;
    case CType::Long:
    case CType::LongLong:
      m_data_uint32.emplace_back(createBuffer<uint32_t>(buffer, numElements, srcComponentType));
      m_ioInfoInMemory.m_componentInfo.m_componentType = CType::UInt32;
      m_ioInfoInMemory.m_componentInfo.m_componentSizeInBytes = 4;
      didCast = true;
      warnSizeConversion = true;
      warnSignConversion = true;
      break;
    case CType::Float32:
    case CType::Float64:
    case CType::LongDouble:
      m_data_uint32.emplace_back(createBuffer<uint32_t>(buffer, numElements, srcComponentType));
      m_ioInfoInMemory.m_componentInfo.m_componentType = CType::UInt32;
      m_ioInfoInMemory.m_componentInfo.m_componentSizeInBytes = 4;
      didCast = true;
      warnFloatConversion = true;
      warnSignConversion = true;
      break;
    case CType::Undefined:
      spdlog::error("Unknown component type in image from file {}", m_ioInfoOnDisk.m_fileInfo.m_fileName);
      return false;
  }

  if (didCast) {
    const std::string newTypeString = componentTypeString(m_ioInfoInMemory.m_componentInfo.m_componentType);

    m_ioInfoInMemory.m_componentInfo.m_componentTypeString = newTypeString;
    m_ioInfoInMemory.m_sizeInfo.m_imageSizeInBytes =
      numElements * m_ioInfoInMemory.m_componentInfo.m_componentSizeInBytes;

    spdlog::info(
      "Casted segmentation {} pixel component from type {} to {}",
      m_ioInfoOnDisk.m_fileInfo.m_fileName,
      m_ioInfoOnDisk.m_componentInfo.m_componentTypeString,
      newTypeString);

    if (warnFloatConversion) {
      spdlog::warn(
        "Floating point to integer conversion: Possible loss of precision and information when "
        "casting segmentation pixel component from type {} to {}",
        m_ioInfoOnDisk.m_componentInfo.m_componentTypeString,
        newTypeString);
    }

    if (warnSizeConversion) {
      spdlog::warn(
        "Size conversion: Possible loss of information when casting segmentation pixel component "
        "from type {} to {}",
        m_ioInfoOnDisk.m_componentInfo.m_componentTypeString,
        newTypeString);
    }

    if (warnSignConversion) {
      spdlog::warn(
        "Signed to unsigned integer conversion: Possible loss of information when casting "
        "segmentation pixel component from type {} to {}",
        m_ioInfoOnDisk.m_componentInfo.m_componentTypeString,
        newTypeString);
    }
  }

  return true;
}

const void* Image::bufferAsVoid(uint32_t comp) const
{
  auto F = [this](uint32_t i) -> const void* {
    switch (m_header.memoryComponentType()) {
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

  switch (m_bufferType) {
    case MultiComponentBufferType::SeparateImages:
      if (m_header.numComponentsPerPixel() <= comp) {
        return nullptr;
      }
      return F(comp);
    case MultiComponentBufferType::InterleavedImage:
      if (1 <= comp) {
        return nullptr;
      }
      return F(0);
  }

  return nullptr;
}

void* Image::bufferAsVoid(uint32_t comp)
{
  return const_cast<void*>(const_cast<const Image*>(this)->bufferAsVoid(comp));
}

const void* Image::bufferSortedAsVoid(uint32_t comp) const
{
  if (m_header.numComponentsPerPixel() <= comp) {
    spdlog::error(
      "Invalid image component {} when retrieving sorted buffer for image with {} components",
      comp,
      m_header.numComponentsPerPixel());
    return nullptr;
  }

  switch (m_header.memoryComponentType()) {
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

std::optional<std::pair<std::size_t, std::size_t>>
Image::getComponentAndOffsetForBuffer(uint32_t comp, int i, int j, int k) const
{
  const glm::u64vec3 dims = m_header.pixelDimensions();

  const std::size_t index =
    dims.x * dims.y * static_cast<std::size_t>(k) + dims.x * static_cast<std::size_t>(j) + static_cast<std::size_t>(i);

  return getComponentAndOffsetForBuffer(comp, index);
}

std::optional<std::pair<std::size_t, std::size_t>> Image::getComponentAndOffsetForBuffer(
  uint32_t comp,
  std::size_t index) const
{
  if (comp >= m_header.numComponentsPerPixel()) {
    spdlog::error("Invalid image component {} (image has {})", comp, m_header.numComponentsPerPixel());
    return std::nullopt;
  }

  switch (m_bufferType) {
    case MultiComponentBufferType::SeparateImages:
      return std::make_pair(comp, index);
    case MultiComponentBufferType::InterleavedImage:
      return std::make_pair(0, m_header.numComponentsPerPixel() * index + comp);
  }

  return std::nullopt;
}
