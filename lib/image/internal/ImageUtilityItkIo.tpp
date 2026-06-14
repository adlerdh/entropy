#pragma once

/**
 * @file ImageUtilityItkIo.tpp
 * @brief Template helpers for reading, writing, splitting, and converting ITK images.
 */

/**
 * @brief Downcast an ITK ImageBase to a scalar ITK image.
 * @tparam ComponentType Pixel component type.
 * @tparam NDim Image dimension.
 * @param[in] imageBase Base image pointer to downcast.
 * @return Typed scalar image pointer, or null if the downcast fails.
 */
template<class ComponentType, uint32_t NDim>
typename itk::Image<ComponentType, NDim>::Pointer downcastImageBaseToImage(
  const typename itk::ImageBase<NDim>::Pointer& imageBase)
{
  typename itk::Image<ComponentType, NDim>::Pointer child =
    dynamic_cast<itk::Image<ComponentType, NDim>*>(imageBase.GetPointer());

  if (!child.GetPointer() || child.IsNull()) {
    spdlog::error("Unable to downcast ImageBase to Image with component type {}", typeid(ComponentType).name());
    return nullptr;
  }

  return child;
}

/**
 * @brief Downcast an ITK ImageBase to a vector ITK image.
 * @tparam ComponentType Pixel component type.
 * @tparam NDim Image dimension.
 * @param[in] imageBase Base image pointer to downcast.
 * @return Typed vector image pointer, or null if the downcast fails.
 */
template<class ComponentType, uint32_t NDim>
typename itk::VectorImage<ComponentType, NDim>::Pointer downcastImageBaseToVectorImage(
  const typename itk::ImageBase<NDim>::Pointer& imageBase)
{
  typename itk::VectorImage<ComponentType, NDim>::Pointer child =
    dynamic_cast<itk::VectorImage<ComponentType, NDim>*>(imageBase.GetPointer());

  if (!child.GetPointer() || child.IsNull()) {
    spdlog::error("Unable to downcast ImageBase to VectorImage with component type {}", typeid(ComponentType).name());
    return nullptr;
  }

  return child;
}

/**
 * @brief Check whether an ITK image base has more than one component per pixel.
 * @tparam NDim Image dimension.
 * @param[in] imageBase Image to inspect.
 * @return True for vector or multi-component images.
 */
template<uint32_t NDim>
bool isVectorImage(const typename itk::ImageBase<NDim>::Pointer& imageBase)
{
  return (imageBase && imageBase.IsNotNull()) ? (imageBase->GetNumberOfComponentsPerPixel() > 1) : false;
}

/**
 * @brief Split a scalar or vector ITK image into scalar component images.
 * @tparam ComponentType Pixel component type.
 * @tparam NDim Image dimension.
 * @param[in] imageBase Scalar or vector image.
 * @return One scalar image per component, or an empty vector on failure.
 *
 * @note Multi-component image data is copied into component images.
 */
template<class ComponentType, uint32_t NDim>
std::vector<typename itk::Image<ComponentType, NDim>::Pointer> splitImageIntoComponents(
  const typename itk::ImageBase<NDim>::Pointer& imageBase)
{
  using ImageType = itk::Image<ComponentType, NDim>;
  using VectorImageType = itk::VectorImage<ComponentType, NDim>;

  std::vector<typename ImageType::Pointer> splitImages;

  if (isVectorImage<NDim>(imageBase)) {
    const typename VectorImageType::Pointer vectorImage =
      downcastImageBaseToVectorImage<ComponentType, NDim>(imageBase);

    if (!vectorImage || vectorImage.IsNull()) {
      spdlog::error("Error casting ImageBase to vector image");
      return splitImages;
    }

    const std::size_t numPixels = vectorImage->GetBufferedRegion().GetNumberOfPixels();
    const uint32_t numComponents = vectorImage->GetVectorLength();

    splitImages.resize(numComponents);

    for (uint32_t i = 0; i < numComponents; ++i) {
      splitImages[i] = ImageType::New();
      splitImages[i]->CopyInformation(vectorImage);
      splitImages[i]->SetRegions(vectorImage->GetBufferedRegion());
      splitImages[i]->Allocate();

      ComponentType* source = vectorImage->GetBufferPointer() + i;
      ComponentType* dest = splitImages[i]->GetBufferPointer();

      const ComponentType* end = dest + numPixels;

      // Copy pixels from component i of vectorImage (the source),
      // which are offset from each other by a stride of numComponents,
      // into pixels of the i'th split image (the destination)
      while (dest < end) {
        *dest = *source;
        ++dest;
        source += numComponents;
      }
    }
  }
  else {
    // Image has only one component
    const auto image = downcastImageBaseToImage<ComponentType, NDim>(imageBase);

    if (!image || image.IsNull()) {
      spdlog::error("Error casting ImageBase to image");
      return splitImages;
    }

    splitImages.resize(1);
    splitImages[0] = image;
  }

  return splitImages;
}

/**
 * @brief Read an image file as a scalar or vector ITK image.
 * @tparam ComponentType Pixel component type to read.
 * @tparam NDim Image dimension.
 * @tparam PixelIsVector True to read as itk::VectorImage, false to read as itk::Image.
 * @param[in] fileName File to read.
 * @return ImageBase pointer, or null on failure.
 */
template<class ComponentType, uint32_t NDim, bool PixelIsVector>
typename itk::ImageBase<NDim>::Pointer readImage(const std::string& fileName)
{
  using ImageType = typename std::
    conditional<PixelIsVector, itk::VectorImage<ComponentType, NDim>, itk::Image<ComponentType, NDim>>::type;

  using ReaderType = itk::ImageFileReader<ImageType>;

  try {
    auto reader = ReaderType::New();
    if (!reader) {
      spdlog::error("Null ITK ImageFileReader on reading image from {}", fileName);
      return nullptr;
    }

    reader->SetFileName(fileName.c_str());
    reader->Update();
    return static_cast<itk::ImageBase<3>::Pointer>(reader->GetOutput());
  }
  catch (const std::exception& e) {
    spdlog::error("Exception reading image from {}: {}", fileName, e.what());
    return nullptr;
  }
}

/**
 * @brief Write an ITK image to disk.
 * @tparam T Pixel component type.
 * @tparam NDim Image dimension.
 * @tparam PixelIsVector True when writing a vector image.
 * @param[in] image ITK image to write.
 * @param[in] fileName Destination file.
 * @return True when the image was written successfully.
 */
template<class T, uint32_t NDim, bool PixelIsVector>
bool writeImage(typename itk::Image<T, NDim>::Pointer image, const std::filesystem::path& fileName)
{
  using ImageType = typename std::conditional<PixelIsVector, itk::VectorImage<T, NDim>, itk::Image<T, NDim>>::type;
  using WriterType = itk::ImageFileWriter<ImageType>;

  if (!image) {
    spdlog::error("Null image cannot be written to {}", fileName);
    return false;
  }

  try {
    auto writer = WriterType::New();
    if (!writer) {
      spdlog::error("Null ITK ImageFileWriter on writing image to '{}'", fileName);
      return false;
    }

    writer->SetFileName(fileName.string().c_str());
    writer->SetInput(image);
    writer->SetUseCompression(true);
    writer->Update();
    return true;
  }
  catch (const std::exception& e) {
    spdlog::error("Exception writing image to '{}': {}", fileName, e.what());
    return false;
  }
}

/**
 * @brief Create an Entropy image from an ITK scalar image.
 *
 * @tparam T Component type of image.
 * @param[in] itkImage ITK image.
 * @param[in] displayName Image display name.
 *
 * @return Entropy image owning a copy of the ITK pixel buffer.
 */
template<class T>
Image createImageFromItkImage(const typename itk::Image<T, 3>::Pointer itkImage, const std::string& displayName)
{
  const auto itkRegion = itkImage->GetLargestPossibleRegion();
  const auto itkSize = itkRegion.GetSize();
  const auto itkSpacing = itkImage->GetSpacing();
  const auto itkOrigin = itkImage->GetOrigin();
  const auto itkDir = itkImage->GetDirection();

  ImageIoInfo info;
  if constexpr (std::is_same_v<T, int8_t>) {
    info.m_componentInfo.m_componentType = ComponentType::Int8;
  }
  else if constexpr (std::is_same_v<T, uint8_t>) {
    info.m_componentInfo.m_componentType = ComponentType::UInt8;
  }
  else if constexpr (std::is_same_v<T, int16_t>) {
    info.m_componentInfo.m_componentType = ComponentType::Int16;
  }
  else if constexpr (std::is_same_v<T, uint16_t>) {
    info.m_componentInfo.m_componentType = ComponentType::UInt16;
  }
  else if constexpr (std::is_same_v<T, int32_t>) {
    info.m_componentInfo.m_componentType = ComponentType::Int32;
  }
  else if constexpr (std::is_same_v<T, uint32_t>) {
    info.m_componentInfo.m_componentType = ComponentType::UInt32;
  }
  else if constexpr (std::is_same_v<T, float>) {
    info.m_componentInfo.m_componentType = ComponentType::Float32;
  }
  else {
    info.m_componentInfo.m_componentType = ComponentType::Undefined;
  }
  info.m_componentInfo.m_componentTypeString = componentTypeString(info.m_componentInfo.m_componentType);
  info.m_componentInfo.m_componentSizeInBytes = sizeof(T);

  info.m_pixelInfo.m_pixelType = PixelType::Scalar;
  info.m_pixelInfo.m_pixelTypeString = "scalar";
  info.m_pixelInfo.m_numComponents = 1;
  info.m_pixelInfo.m_pixelStrideInBytes = sizeof(T);

  const std::size_t N = itkSize[0] * itkSize[1] * itkSize[2];
  info.m_sizeInfo.m_imageSizeInComponents = N;
  info.m_sizeInfo.m_imageSizeInPixels = N;
  info.m_sizeInfo.m_imageSizeInBytes = N * sizeof(T);

  info.m_spaceInfo.m_numDimensions = 3;
  info.m_spaceInfo.m_dimensions = {itkSize[0], itkSize[1], itkSize[2]};
  info.m_spaceInfo.m_origin = {itkOrigin[0], itkOrigin[1], itkOrigin[2]};
  info.m_spaceInfo.m_spacing = {itkSpacing[0], itkSpacing[1], itkSpacing[2]};
  info.m_spaceInfo.m_directions = {
    {itkDir[0][0], itkDir[1][0], itkDir[2][0]},
    {itkDir[0][1], itkDir[1][1], itkDir[2][1]},
    {itkDir[0][2], itkDir[1][2], itkDir[2][2]}};

  const glm::uvec3 dims{itkSize[0], itkSize[1], itkSize[2]};
  const glm::vec3 origin{itkOrigin[0], itkOrigin[1], itkOrigin[2]};
  const glm::vec3 spacing{itkSpacing[0], itkSpacing[1], itkSpacing[2]};

  // Set matrix of direction vectors in column-major order
  const glm::mat3 dir{
    itkDir[0][0],
    itkDir[0][1],
    itkDir[0][2],
    itkDir[1][0],
    itkDir[1][1],
    itkDir[1][2],
    itkDir[2][0],
    itkDir[2][1],
    itkDir[2][2]};

  ImageHeader header(info, info, false);
  header.setFileName("<none>");
  header.setExistsOnDisk(false);
  header.setHeaderOverrides(ImageHeaderOverrides(dims, spacing, origin, dir));

  Image image(
    header,
    displayName,
    Image::ImageRepresentation::Image,
    Image::MultiComponentBufferType::SeparateImages,
    std::vector<const void*>{itkImage->GetBufferPointer()});

  return image;
}
