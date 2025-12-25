#include "DistanceMap.h"

#include "common/UuidUtility.h"
#include "image/ImageUtility.tpp"

#include <spdlog/fmt/ostr.h>
#include <spdlog/spdlog.h>

void createNoiseEstimates(const Image& image, const uuids::uuid& imageUid, AppData& data)
{
  using TItkImageComp = float;
  constexpr uint32_t radius = 1;

  for (uint32_t comp = 0; comp < image.header().numComponentsPerPixel(); ++comp)
  {
    const auto compImage = createItkImageFromImageComponent<TItkImageComp>(image, comp);
    const auto noiseEstimateItkImage = computeNoiseEstimate<TItkImageComp>(compImage, radius);
    if (!noiseEstimateItkImage)
    {
      spdlog::warn("Unable to create noise estimate for component {} of image {}", comp, imageUid);
      continue;
    }

    const std::string displayName = std::string("Noise estimate for comp ") + std::to_string(comp) +
                                    " of '" + image.settings().displayName() + "'";

    Image noiseEstimateImage = createImageFromItkImage<TItkImageComp>(noiseEstimateItkImage, displayName);
    const glm::uvec3 noiseImgSize = noiseEstimateImage.header().pixelDimensions();

    // data.addImage(noiseEstimateImage); // Debug purposes
    data.addNoiseEstimate(imageUid, comp, std::move(noiseEstimateImage), radius);

    spdlog::debug("Created noise estimate ({}x{}x{} voxels) with radius {} for component {} of image {}",
                  noiseImgSize.x, noiseImgSize.y, noiseImgSize.z, radius, comp, imageUid);
  }
}

void createDistanceMaps(const Image& image, const uuids::uuid& imageUid,
                        float downsamplingFactor, AppData& data)
{
  // If the image has multiple, interleaved components, then do not compute the distance map
  // for the components, since we have not yet written functions to perform distance map
  // calculations on images with interleaved components.
  if (image.header().interleavedComponents()) {
    spdlog::info("Image {} has multiple, interleaved components, "
                 "so the distance map will not be computed", imageUid);
    return;
  }

  // Create ITK images with float components from which distance maps and noise estimates are computed
  using TItkImageComp = float;
  using TDistMapComp = uint8_t; // to save GPU memory

  for (uint32_t comp = 0; comp < image.header().numComponentsPerPixel(); ++comp)
  {
    /// @note It is somewhat wasteful to recreate an ITK image for each component,
    /// especially since the image was originally loaded using ITK. But the utility
    /// functions that we use require an ITK image as input.

    const auto compImage = createItkImageFromImageComponent<TItkImageComp>(image, comp);

    // Compute foreground distance map for image component:
    const auto thresholds = image.settings().foregroundThresholds(comp);
    const float threshLow = static_cast<float>(thresholds.first);
    const float threshHigh = static_cast<float>(thresholds.second);

    spdlog::debug("Computing Euclidean distance map for image {} using thresholds {} and {}",
                  imageUid, threshLow, threshHigh);

    const auto distMapItkImage = computeEuclideanDistanceMap<TItkImageComp, TDistMapComp>(
      compImage, comp, threshLow, threshHigh, std::min(downsamplingFactor, 1.0f));

    if (!distMapItkImage) {
      spdlog::warn("Unable to create distance map for component {} of image {}", comp, imageUid);
      continue;
    }

    const std::string displayName = std::string("Dist map for comp ") + std::to_string(comp) +
                                    " of '" + image.settings().displayName() + "'";

    Image distMapImage = createImageFromItkImage<TDistMapComp>(distMapItkImage, displayName);
    const glm::uvec3 distMapSize = distMapImage.header().pixelDimensions();

    spdlog::debug("Created distance map ({}x{}x{} voxels) to foreground region [{}, {}] of component {} "
                  "of image {}", distMapSize.x, distMapSize.y, distMapSize.z, threshLow, threshHigh, comp, imageUid);

    // data.addImage(distMapImage); // Debug purposes
    data.addDistanceMap(imageUid, comp, std::move(distMapImage), thresholds.second);
  }
}
