#pragma once

/**
 * @file ImageUtilityLoad.tpp
 * @brief Template dispatcher for loading scalar and vector image buffers from disk.
 */

/**
 * @brief Load scalar or vector image data from disk and forward raw buffers to a caller callback.
 * @tparam ReadComponentType Component type used by ITK while reading.
 * @param[in] fileName Image file to load.
 * @param[in] numPixels Expected number of pixels.
 * @param[in] numComps Number of components on disk.
 * @param[in] numCompsToLoad Number of components to load into memory.
 * @param[in] isVectorImage Whether the on-disk image has vector pixels.
 * @param[in] bufferType Desired in-memory multi-component buffer layout.
 * @param[in] loadBuffer Callback that stores each raw loaded buffer.
 * @return True when all requested image data was loaded.
 */
template<typename ReadComponentType>
bool loadImage(
  const std::filesystem::path& fileName,
  std::size_t numPixels,
  uint32_t numComps,
  uint32_t numCompsToLoad,
  bool isVectorImage,
  const Image::MultiComponentBufferType bufferType,
  std::function<bool(const void* buffer, std::size_t numElements)> loadBuffer)
{
  (void)numComps;
  using ReadImageType = itk::Image<ReadComponentType, 3>;

  if (isVectorImage) {
    // Load multi-component image
    constexpr bool pixelIsVector = true;
    typename itk::ImageBase<3>::Pointer baseImage = readImage<ReadComponentType, 3, pixelIsVector>(fileName.string());
    if (!baseImage) {
      spdlog::error("Unable to read vector ImageBase for image {}", fileName);
      return false;
    }

    // Split the base image into component images. Load a maximum of MAX_COMPS components
    // for an image with interleaved component buffers.
    std::vector<typename ReadImageType::Pointer> componentImages =
      splitImageIntoComponents<ReadComponentType, 3>(baseImage);

    if (componentImages.size() < numCompsToLoad) {
      spdlog::error(
        "Only {} image components were loaded, but {} components were expected",
        componentImages.size(),
        numCompsToLoad);
      return false;
    }

    // If interleaving vector components, then create a single buffer
    std::unique_ptr<ReadComponentType[]> allComponentBuffers = nullptr;

    if (Image::MultiComponentBufferType::InterleavedImage == bufferType) {
      allComponentBuffers = std::make_unique<ReadComponentType[]>(numPixels * numCompsToLoad);
      if (!allComponentBuffers) {
        spdlog::error("Null buffer holding all components of image file {}", fileName);
        return false;
      }
    }

    // Load the buffers from the component images
    for (uint32_t i = 0; i < numCompsToLoad; ++i) {
      if (!componentImages[i]) {
        spdlog::error("Null vector image component {} for image file {}", i, fileName);
        return false;
      }

      const ReadComponentType* buffer = componentImages[i]->GetBufferPointer();
      if (!buffer) {
        spdlog::error("Null buffer of vector image component {} for image file {}", i, fileName);
        return false;
      }

      switch (bufferType) {
        case Image::MultiComponentBufferType::SeparateImages: {
          if (!loadBuffer(static_cast<const void*>(buffer), numPixels)) {
            spdlog::error("Error loading separated image component buffer {} for image file {}", i, fileName);
            return false;
          }
          break;
        }
        case Image::MultiComponentBufferType::InterleavedImage: {
          // Fill the interleaved buffer
          for (std::size_t p = 0; p < numPixels; ++p) {
            allComponentBuffers[numCompsToLoad * p + i] = buffer[p];
          }
          break;
        }
      }
    }

    if (Image::MultiComponentBufferType::InterleavedImage == bufferType) {
      const std::size_t numElements = numPixels * numCompsToLoad;
      if (!loadBuffer(static_cast<const void*>(allComponentBuffers.get()), numElements)) {
        spdlog::error("Error loading interleaved buffer for image file {}", fileName);
        return false;
      }
    }
  }
  else {
    // Load scalar, single-component image
    constexpr bool pixelIsVector = false;

    typename itk::ImageBase<3>::Pointer baseImage = readImage<ReadComponentType, 3, pixelIsVector>(fileName.string());
    if (!baseImage) {
      spdlog::error("Unable to read ImageBase from file {}", fileName);
      return false;
    }

    typename ReadImageType::Pointer image = downcastImageBaseToImage<ReadComponentType, 3>(baseImage);
    if (!image) {
      spdlog::error("Null image for file {} following downcast from ImageBase", fileName);
      return false;
    }

    const ReadComponentType* buffer = image->GetBufferPointer();
    if (!buffer) {
      spdlog::error("Null buffer of scalar image file {}", fileName);
      return false;
    }

    if (!loadBuffer(static_cast<const void*>(buffer), numPixels)) {
      spdlog::error("Error loading buffer for image file {}", fileName);
      return false;
    }
  }

  return true;
}
