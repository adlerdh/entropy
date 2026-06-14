#pragma once

/**
 * @file ImageUtilityItkImageConstruction.tpp
 * @brief Template helpers for constructing ITK scalar and vector images from Entropy buffers.
 */

/**
 * @brief Wrap a contiguous scalar component buffer as a 3D ITK image.
 * @tparam T Pixel component type.
 * @param[in] imageDims Image dimensions in pixels.
 * @param[in] imageOrigin Image origin in physical coordinates.
 * @param[in] imageSpacing Pixel spacing.
 * @param[in] imageDirection Direction matrix stored as row vectors.
 * @param[in] imageData Contiguous scalar pixel buffer.
 * @return ITK image that views \p imageData, or null on invalid input.
 */
template<class T>
typename itk::Image<T, 3>::Pointer makeScalarImage(
  const std::array<uint32_t, 3>& imageDims,
  const std::array<double, 3>& imageOrigin,
  const std::array<double, 3>& imageSpacing,
  const std::array<std::array<double, 3>, 3>& imageDirection,
  const T* imageData)
{
  using ImportFilterType = itk::ImportImageFilter<T, 3>;

  if (!imageData) {
    spdlog::error("Null data array provided when creating new scalar image");
    return nullptr;
  }

  // This filter will not free the memory in its destructor and the application providing the
  // buffer retains the responsibility of freeing the memory for this image data
  constexpr bool filterOwnsBuffer = false;

  typename ImportFilterType::IndexType start;
  typename ImportFilterType::SizeType size;
  typename ImportFilterType::DirectionType direction;

  itk::SpacePrecisionType origin[3];
  itk::SpacePrecisionType spacing[3];

  for (uint32_t i = 0; i < 3; ++i) {
    start[i] = 0.0;
    size[i] = imageDims[i];
    origin[i] = imageOrigin[i];
    spacing[i] = imageSpacing[i];

    for (uint32_t j = 0; j < 3; ++j) {
      direction[i][j] = imageDirection[j][i];
    }
  }

  const std::size_t numPixels = size[0] * size[1] * size[2];

  if (0 == numPixels) {
    spdlog::error("Cannot create new scalar image with size zero");
    return nullptr;
  }

  typename ImportFilterType::RegionType region;
  region.SetIndex(start);
  region.SetSize(size);

  try {
    auto importer = ImportFilterType::New();
    importer->SetRegion(region);
    importer->SetOrigin(origin);
    importer->SetSpacing(spacing);
    importer->SetDirection(direction);
    importer->SetImportPointer(const_cast<T*>(imageData), numPixels, filterOwnsBuffer);
    importer->Update();
    return importer->GetOutput();
  }
  catch (const std::exception& e) {
    spdlog::error("Exception creating new ITK scalar image from data array: {}", e.what());
    return nullptr;
  }
}

/**
 * @brief Wrap an interleaved vector component buffer as a 3D ITK vector image.
 * @tparam T Pixel component type.
 * @tparam VectorDim Number of components per pixel.
 * @param[in] imageDims Image dimensions in pixels.
 * @param[in] imageOrigin Image origin in physical coordinates.
 * @param[in] imageSpacing Pixel spacing.
 * @param[in] imageDirection Direction matrix stored as row vectors.
 * @param[in] imageData Contiguous interleaved vector pixel buffer.
 * @return ITK vector image that views \p imageData, or null on invalid input.
 */
template<class T, uint32_t VectorDim>
typename itk::Image<itk::Vector<T, VectorDim>, 3>::Pointer makeVectorImage(
  const std::array<uint32_t, 3>& imageDims,
  const std::array<double, 3>& imageOrigin,
  const std::array<double, 3>& imageSpacing,
  const std::array<std::array<double, 3>, 3>& imageDirection,
  const T* imageData)
{
  using VectorPixelType = itk::Vector<T, VectorDim>;
  using ImportFilterType = itk::ImportImageFilter<VectorPixelType, 3>;

  if (!imageData) {
    spdlog::error("Null data array provided when creating new vector image");
    return nullptr;
  }

  // This filter will not free the memory in its destructor and the application providing the
  // buffer retains the responsibility of freeing the memory for this image data
  constexpr bool filterOwnsBuffer = false;

  typename ImportFilterType::IndexType start;
  typename ImportFilterType::SizeType size;
  typename ImportFilterType::DirectionType direction;

  itk::SpacePrecisionType origin[3];
  itk::SpacePrecisionType spacing[3];

  for (uint32_t i = 0; i < 3; ++i) {
    start[i] = 0.0;
    size[i] = imageDims[i];
    origin[i] = imageOrigin[i];
    spacing[i] = imageSpacing[i];

    for (uint32_t j = 0; j < 3; ++j) {
      direction[i][j] = imageDirection[j][i];
    }
  }

  const std::size_t numPixels = size[0] * size[1] * size[2];

  if (0 == numPixels) {
    spdlog::error("Cannot create new vector image with size zero");
    return nullptr;
  }

  typename ImportFilterType::RegionType region;
  region.SetIndex(start);
  region.SetSize(size);

  try {
    auto importer = ImportFilterType::New();
    importer->SetRegion(region);
    importer->SetOrigin(origin);
    importer->SetSpacing(spacing);
    importer->SetDirection(direction);
    importer->SetImportPointer(
      reinterpret_cast<VectorPixelType*>(const_cast<T*>(imageData)),
      numPixels,
      filterOwnsBuffer);
    importer->Update();

    return importer->GetOutput();
  }
  catch (const std::exception& e) {
    spdlog::error("Exception creating new ITK vector image from data array: {}", e.what());
    return nullptr;
  }
}

/**
 * @brief Create a scalar ITK image from one Entropy image component.
 *
 * @tparam T Component type of output image.
 * @param[in] image Entropy image.
 * @param[in] component Image component.
 *
 * @return Scalar ITK image of the component, or null on invalid input.
 */
template<class T>
typename itk::Image<T, 3>::Pointer createItkImageFromImageComponent(const Image& image, uint32_t component)
{
  using OutputImageType = itk::Image<T, 3>;
  const ImageHeader& header = image.header();

  if (component >= header.numComponentsPerPixel()) {
    spdlog::error(
      "Invalid image component {} to convert to ITK image; image has only {} components",
      component,
      header.numComponentsPerPixel());
    return nullptr;
  }

  std::array<uint32_t, 3> dims;
  std::array<double, 3> origin;
  std::array<double, 3> spacing;
  std::array<std::array<double, 3>, 3> directions;

  for (uint32_t i = 0; i < 3; ++i) {
    const int ii = static_cast<int>(i);
    dims[i] = header.pixelDimensions()[ii];
    origin[i] = static_cast<double>(header.origin()[ii]);
    spacing[i] = static_cast<double>(header.spacing()[ii]);

    directions[i] = {
      static_cast<double>(header.directions()[ii].x),
      static_cast<double>(header.directions()[ii].y),
      static_cast<double>(header.directions()[ii].z)};
  }

  switch (header.memoryComponentType()) {
    case ComponentType::Int8: {
      using S = int8_t;
      using InputImageType = itk::Image<S, 3>;
      using FilterType = itk::CastImageFilter<InputImageType, OutputImageType>;
      auto caster = FilterType::New();

      InputImageType::Pointer compImage =
        makeScalarImage(dims, origin, spacing, directions, static_cast<const S*>(image.bufferAsVoid(component)));

      if (!compImage) {
        return nullptr;
      }
      caster->SetInput(compImage);
      caster->Update();
      return caster->GetOutput();
    }
    case ComponentType::UInt8: {
      using S = uint8_t;
      using InputImageType = itk::Image<S, 3>;
      using FilterType = itk::CastImageFilter<InputImageType, OutputImageType>;
      typename FilterType::Pointer caster = FilterType::New();

      InputImageType::Pointer compImage =
        makeScalarImage(dims, origin, spacing, directions, static_cast<const S*>(image.bufferAsVoid(component)));

      if (!compImage) {
        return nullptr;
      }

      caster->SetInput(compImage);
      caster->Update();
      return caster->GetOutput();
    }
    case ComponentType::Int16: {
      using S = int16_t;
      using InputImageType = itk::Image<S, 3>;
      using FilterType = itk::CastImageFilter<InputImageType, OutputImageType>;
      typename FilterType::Pointer caster = FilterType::New();

      InputImageType::Pointer compImage =
        makeScalarImage(dims, origin, spacing, directions, static_cast<const S*>(image.bufferAsVoid(component)));

      if (!compImage) {
        return nullptr;
      }
      caster->SetInput(compImage);
      caster->Update();
      return caster->GetOutput();
    }
    case ComponentType::UInt16: {
      using S = uint16_t;
      using InputImageType = itk::Image<S, 3>;
      using FilterType = itk::CastImageFilter<InputImageType, OutputImageType>;
      typename FilterType::Pointer caster = FilterType::New();

      InputImageType::Pointer compImage =
        makeScalarImage(dims, origin, spacing, directions, static_cast<const S*>(image.bufferAsVoid(component)));

      if (!compImage) {
        return nullptr;
      }
      caster->SetInput(compImage);
      caster->Update();
      return caster->GetOutput();
    }
    case ComponentType::Int32: {
      using S = int32_t;
      using InputImageType = itk::Image<S, 3>;
      using FilterType = itk::CastImageFilter<InputImageType, OutputImageType>;
      typename FilterType::Pointer caster = FilterType::New();

      InputImageType::Pointer compImage =
        makeScalarImage(dims, origin, spacing, directions, static_cast<const S*>(image.bufferAsVoid(component)));

      if (!compImage) {
        return nullptr;
      }
      caster->SetInput(compImage);
      caster->Update();
      return caster->GetOutput();
    }
    case ComponentType::UInt32: {
      using S = uint32_t;
      using InputImageType = itk::Image<S, 3>;
      using FilterType = itk::CastImageFilter<InputImageType, OutputImageType>;
      typename FilterType::Pointer caster = FilterType::New();

      InputImageType::Pointer compImage =
        makeScalarImage(dims, origin, spacing, directions, static_cast<const S*>(image.bufferAsVoid(component)));

      if (!compImage) {
        return nullptr;
      }
      caster->SetInput(compImage);
      caster->Update();
      return caster->GetOutput();
    }
    case ComponentType::Float32: {
      using S = float;
      using InputImageType = itk::Image<S, 3>;
      using FilterType = itk::CastImageFilter<InputImageType, OutputImageType>;
      typename FilterType::Pointer caster = FilterType::New();

      InputImageType::Pointer compImage =
        makeScalarImage(dims, origin, spacing, directions, static_cast<const S*>(image.bufferAsVoid(component)));

      if (!compImage) {
        return nullptr;
      }
      caster->SetInput(compImage);
      caster->Update();
      return caster->GetOutput();
    }
    default: {
      spdlog::error(
        "Invalid image component type '{}' upon conversion of component to ITK image",
        header.memoryComponentTypeAsString());
      return nullptr;
    }
  }

  return nullptr;
}
