#pragma once

/**
 * @file ImageUtilityDerivedImages.tpp
 * @brief Template helpers for derived image filters such as noise and distance maps.
 */

/**
 * @brief Compute a local noise estimate image with ITK's noise filter.
 * @tparam T Pixel component type.
 * @param[in] image Input image.
 * @param[in] radius Neighborhood radius in voxels.
 * @return Noise estimate image, or null if ITK fails.
 */
template<typename T>
typename itk::Image<T, 3>::Pointer computeNoiseEstimate(const typename itk::Image<T, 3>::Pointer image, uint32_t radius)
{
  using ImageType = itk::Image<T, 3>;
  using NoiseImageFilterType = itk::NoiseImageFilter<ImageType, ImageType>;

  auto noiseFilter = NoiseImageFilterType::New();
  noiseFilter->SetInput(image);
  noiseFilter->SetRadius(radius);
  noiseFilter->Update();

  return noiseFilter->GetOutput();
}

/**
 * @brief Compute the signed distance transformation to the boundary of an image.
 * -Voxels inside of the boundary are defined to have negative distance,
 * -Voxels outside of the boundary are defined to have positive distance.
 * -Voxels on the boundary have zero distance.
 *
 * @tparam T Component type of the input image
 * @tparam U Component type of the output distance map image. If U is a signed type,
 * then negative distances are returned for voxels inside the boundary. If U is an
 * unsigned type, then zero distance is returned for voxels inside inside the boundary.
 *
 * @param[in] image Input image
 * @param[in] component Image component
 * @param[in] lowerBoundaryValue Lower value of boundary in input image
 * @param[in] upperBoundaryValue Upper value of boundary in input image
 * @param[in] downsampleFactor Downsampling factor in range (0, 1]. The output distance map
 * will be downsampled by this factor compared to the input image. E.g.: If the factor is 0.25f,
 * then the distance map will be 4x smaller than the input image in each dimension.
 *
 * @return Output distance map image
 */
template<typename T, typename U>
typename itk::Image<U, 3>::Pointer computeEuclideanDistanceMap(
  const typename itk::Image<T, 3>::Pointer image,
  uint32_t component,
  const T& lowerBoundaryValue,
  const T& upperBoundaryValue,
  float downsampleFactor)
{
  using Timer = std::chrono::time_point<std::chrono::system_clock>;

  using TInputImage = itk::Image<T, 3>;
  using TDistImage = itk::Image<U, 3>;
  using TBoundaryImage = itk::Image<uint8_t, 3>;
  using TFloatImage = itk::Image<float, 3>;

  if (!image) {
    spdlog::error("Input image is null when computing Euclidean distance transformation");
    return nullptr;
  }

  float scale = downsampleFactor;

  if (downsampleFactor <= 0.0f || 1.0f < downsampleFactor) {
    spdlog::warn(
      "Invalid downsampling factor {} provided to Euclidean distance transformation; "
      "using 1.0 (no downsampling) instead",
      downsampleFactor);
    scale = 1.0f;
  }

  // Binarize the original image, with values 1 inside and 0 outside
  auto thresholdFilter = itk::BinaryThresholdImageFilter<TInputImage, TInputImage>::New();
  thresholdFilter->SetInput(image);
  thresholdFilter->SetLowerThreshold(lowerBoundaryValue);
  thresholdFilter->SetUpperThreshold(upperBoundaryValue);
  thresholdFilter->SetOutsideValue(0);
  thresholdFilter->SetInsideValue(1);

  // Downsample the thresholded boundary image in order to reduce the size of the resulting distance map.
  const auto inputRegion = image->GetLargestPossibleRegion();
  const auto inputSize = inputRegion.GetSize();
  const auto inputSpacing = image->GetSpacing();
  const auto inputOrigin = image->GetOrigin();
  const auto inputDir = image->GetDirection();

  typename TInputImage::SizeType outputSize;
  typename TInputImage::SpacingType outputSpacing;
  typename TInputImage::PointType outputOrigin;

  for (uint32_t i = 0; i < 3; ++i) {
    // 1 is the minimum value for any dimension:
    outputSize[i] = std::max(static_cast<std::size_t>(inputSize[i] * scale), static_cast<std::size_t>(1ul));
  }

  itk::Vector<double, 3> delta;

  for (uint32_t i = 0; i < 3; ++i) {
    // Each axis can clamp to a different output size, especially for planar/slab images. Derive spacing from the
    // actual axis scale so the downsampled distance map stays geometrically aligned with the source image.
    const double axisScale = static_cast<double>(outputSize[i]) / static_cast<double>(inputSize[i]);
    outputSpacing[i] = inputSpacing[i] / axisScale;
    delta[i] = 0.5 * (outputSpacing[i] - inputSpacing[i]);
  }

  outputOrigin = inputOrigin + inputDir * delta;

  using TCoord = double;
  auto interpolator = itk::LinearInterpolateImageFunction<TInputImage, TCoord>::New();

  // Resample to floating point image type, so that partial voluming can be correctly resolved
  // with a subsequent ceiling filter
  auto resampleFilter = itk::ResampleImageFilter<TInputImage, TFloatImage>::New();
  resampleFilter->SetInput(thresholdFilter->GetOutput());
  resampleFilter->SetInterpolator(interpolator);
  resampleFilter->SetSize(outputSize);
  resampleFilter->SetOutputSpacing(outputSpacing);
  resampleFilter->SetOutputOrigin(outputOrigin);
  resampleFilter->SetOutputDirection(image->GetDirection());
  resampleFilter->SetDefaultPixelValue(0);

  // Compute the ceiling of the resampled values, so that any value even slightly larger than
  // zero gets mapped to one (inside the boundary). That way the boundary is never underestimated.
  auto ceilFilter = itk::BinaryThresholdImageFilter<TFloatImage, TBoundaryImage>::New();
  ceilFilter->SetInput(resampleFilter->GetOutput());
  ceilFilter->SetLowerThreshold(0.0f);
  ceilFilter->SetUpperThreshold(0.0f);
  ceilFilter->SetOutsideValue(1);
  ceilFilter->SetInsideValue(0);

  const Timer startThreshold = std::chrono::system_clock::now();
  ceilFilter->Update();
  const Timer stopThreshold = std::chrono::system_clock::now();

  spdlog::debug(
    "Took {} msec to compute image threshold, resampling, and ceiling",
    std::chrono::duration_cast<std::chrono::milliseconds>(stopThreshold - startThreshold).count());

  // Compute the distance map in mm from every voxel to the boundary. Distances are computed for
  // voxels that are both inside and outside the boundary.
  auto distanceFilter = itk::SignedMaurerDistanceMapImageFilter<TBoundaryImage, TFloatImage>::New();
  distanceFilter->SetInput(ceilFilter->GetOutput());
  distanceFilter->UseImageSpacingOn();
  distanceFilter->SquaredDistanceOff();

  const Timer startDistance = std::chrono::system_clock::now();
  distanceFilter->Update();
  const Timer stopDistance = std::chrono::system_clock::now();

  spdlog::debug(
    "Took {} msec to compute distance map to resampled boundary",
    std::chrono::duration_cast<std::chrono::milliseconds>(stopDistance - startDistance).count());

  auto distImage = distanceFilter->GetOutput();

  // If casting to an integral type, then ceil negative values and floor positive values.
  // This is so that distance to the boundary is never overestimated in the returned image.

  if (std::is_integral<U>()) {
    itk::ImageRegionIterator<TFloatImage> it(distImage, distImage->GetLargestPossibleRegion());
    it.GoToBegin();

    while (!it.IsAtEnd()) {
      const float d = it.Get();
      it.Set((d < 0.0f) ? std::ceil(d) : std::floor(d));
      ++it;
    }
  }

  // Clamp and cast pixels to the range of the output image type TDistImage (U).
  // Note that by default, the clamp bounds equal the range supported by type U.
  auto clampFilter = itk::ClampImageFilter<TFloatImage, TDistImage>::New();
  clampFilter->SetInput(distImage);
  clampFilter->Update();

  if (DEBUG_IMAGE_OUTPUT) {
    const std::string ending = "_" + std::to_string(component) + ".nii.gz";
    writeImage<T, 3, false>(image, "0.image.nii.gz");
    writeImage<T, 3, false>(thresholdFilter->GetOutput(), "1.thresh" + ending);
    writeImage<float, 3, false>(resampleFilter->GetOutput(), "2.resample" + ending);
    writeImage<uint8_t, 3, false>(ceilFilter->GetOutput(), "3.ceiling" + ending);
    writeImage<float, 3, false>(distanceFilter->GetOutput(), "4.distance" + ending);
    writeImage<U, 3, false>(clampFilter->GetOutput(), "5.distance_clamp" + ending);
  }

  return clampFilter->GetOutput();
}
