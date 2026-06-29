#pragma once

/**
 * @file ImageUtilityLoad.tpp
 * @brief Template dispatcher for loading scalar and vector image buffers from disk.
 */

namespace image_utility_load_detail
{
template<typename ReadComponentType, uint32_t Dimension>
bool loadImageAtDimension(
  const std::filesystem::path& fileName,
  std::size_t numPixels,
  uint32_t componentsToLoad,
  bool isMultiComponentImage,
  const Image::MultiComponentBufferType bufferType,
  std::function<bool(const void* buffer, std::size_t numElements)> loadBuffer)
{
  using ReadImageType = itk::Image<ReadComponentType, Dimension>;

  if (isMultiComponentImage) {
    constexpr bool pixelIsVector = true;
    typename itk::ImageBase<Dimension>::Pointer baseImage =
      readImage<ReadComponentType, Dimension, pixelIsVector>(fileName.string());
    if (!baseImage) {
      spdlog::error("Unable to read {}D vector ImageBase for image {}", Dimension, fileName);
      return false;
    }

    std::vector<typename ReadImageType::Pointer> componentImages =
      splitImageIntoComponents<ReadComponentType, Dimension>(baseImage);

    if (componentImages.size() < componentsToLoad) {
      spdlog::error(
        "Only {} image component(s) were loaded from {}, but {} component(s) were expected",
        componentImages.size(),
        fileName,
        componentsToLoad);
      return false;
    }

    std::unique_ptr<ReadComponentType[]> allComponentBuffers = nullptr;
    if (Image::MultiComponentBufferType::InterleavedImage == bufferType) {
      allComponentBuffers = std::make_unique<ReadComponentType[]>(numPixels * componentsToLoad);
      if (!allComponentBuffers) {
        spdlog::error("Null buffer holding all components of image file {}", fileName);
        return false;
      }
    }

    for (uint32_t component = 0; component < componentsToLoad; ++component) {
      const ReadComponentType* buffer =
        componentImages[component] ? componentImages[component]->GetBufferPointer() : nullptr;
      if (!buffer) {
        spdlog::error("Null buffer of image component {} for image file {}", component, fileName);
        return false;
      }

      switch (bufferType) {
        case Image::MultiComponentBufferType::SeparateImages: {
          if (!loadBuffer(static_cast<const void*>(buffer), numPixels)) {
            spdlog::error("Error loading separated image component buffer {} for image file {}", component, fileName);
            return false;
          }
          break;
        }
        case Image::MultiComponentBufferType::InterleavedImage: {
          for (std::size_t pixel = 0; pixel < numPixels; ++pixel) {
            allComponentBuffers[componentsToLoad * pixel + component] = buffer[pixel];
          }
          break;
        }
      }
    }

    if (Image::MultiComponentBufferType::InterleavedImage == bufferType) {
      const std::size_t numElements = numPixels * componentsToLoad;
      if (!loadBuffer(static_cast<const void*>(allComponentBuffers.get()), numElements)) {
        spdlog::error("Error loading interleaved image buffer for image file {}", fileName);
        return false;
      }
    }

    return true;
  }

  constexpr bool pixelIsVector = false;
  typename itk::ImageBase<Dimension>::Pointer baseImage =
    readImage<ReadComponentType, Dimension, pixelIsVector>(fileName.string());
  if (!baseImage) {
    spdlog::error("Unable to read {}D ImageBase from file {}", Dimension, fileName);
    return false;
  }

  typename ReadImageType::Pointer image = downcastImageBaseToImage<ReadComponentType, Dimension>(baseImage);
  if (!image) {
    spdlog::error("Null {}D image for file {} following downcast from ImageBase", Dimension, fileName);
    return false;
  }

  const ReadComponentType* buffer = image->GetBufferPointer();
  if (!buffer) {
    spdlog::error("Null buffer of scalar image file {}", fileName);
    return false;
  }

  if (!loadBuffer(static_cast<const void*>(buffer), numPixels)) {
    spdlog::error("Error loading scalar buffer for image file {}", fileName);
    return false;
  }

  return true;
}
} // namespace image_utility_load_detail

/**
 * @brief Load scalar or vector image data from disk and forward raw buffers to a caller callback.
 * @tparam ReadComponentType Component type used by ITK while reading.
 * @param[in] fileName Image file to load.
 * @param[in] numDimensions Number of image dimensions to request from ITK.
 * @param[in] numPixels Expected number of pixels in the loaded file buffer.
 * @param[in] componentsToLoad Number of logical components to load into memory.
 * @param[in] isMultiComponentImage Whether the on-disk image has more than one component per pixel.
 * @param[in] bufferType Desired in-memory multi-component buffer layout.
 * @param[in] loadBuffer Callback that stores each raw loaded buffer.
 * @return True when all requested image data was loaded.
 */
template<typename ReadComponentType>
bool loadImage(
  const std::filesystem::path& fileName,
  uint32_t numDimensions,
  std::size_t numPixels,
  uint32_t componentsToLoad,
  bool isMultiComponentImage,
  const Image::MultiComponentBufferType bufferType,
  std::function<bool(const void* buffer, std::size_t numElements)> loadBuffer)
{
  switch (numDimensions) {
    case 1u:
      return image_utility_load_detail::loadImageAtDimension<ReadComponentType, 1>(
        fileName,
        numPixels,
        componentsToLoad,
        isMultiComponentImage,
        bufferType,
        std::move(loadBuffer));
    case 2u:
      return image_utility_load_detail::loadImageAtDimension<ReadComponentType, 2>(
        fileName,
        numPixels,
        componentsToLoad,
        isMultiComponentImage,
        bufferType,
        std::move(loadBuffer));
    case 3u:
      return image_utility_load_detail::loadImageAtDimension<ReadComponentType, 3>(
        fileName,
        numPixels,
        componentsToLoad,
        isMultiComponentImage,
        bufferType,
        std::move(loadBuffer));
    case 4u:
      return image_utility_load_detail::loadImageAtDimension<ReadComponentType, 4>(
        fileName,
        numPixels,
        componentsToLoad,
        isMultiComponentImage,
        bufferType,
        std::move(loadBuffer));
    default:
      spdlog::error("Unsupported image dimension {} when loading image {}", numDimensions, fileName);
      return false;
  }
}
