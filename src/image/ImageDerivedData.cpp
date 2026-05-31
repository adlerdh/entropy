#include "ImageDerivedData.h"
#include "internal/ImageUtility.tpp"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <string>
#include <utility>

std::vector<ComponentImageResult> createNoiseEstimateImages(const Image& image, uint32_t radius)
{
  using TItkImageComp = float;

  std::vector<ComponentImageResult> results;
  results.reserve(image.header().numComponentsPerPixel());

  for (uint32_t comp = 0; comp < image.header().numComponentsPerPixel(); ++comp) {
    const auto compImage = createItkImageFromImageComponent<TItkImageComp>(image, comp);
    const auto noiseEstimateItkImage = computeNoiseEstimate<TItkImageComp>(compImage, radius);
    if (!noiseEstimateItkImage) {
      spdlog::warn("Unable to create noise estimate for component {}", comp);
      continue;
    }

    const std::string displayName =
      std::string("Noise estimate for comp ") + std::to_string(comp) + " of '" + image.settings().displayName() + "'";

    results.push_back(
      ComponentImageResult{comp, createImageFromItkImage<TItkImageComp>(noiseEstimateItkImage, displayName)});
  }

  return results;
}

std::vector<DistanceMapImageResult> createDistanceMapImages(const Image& image, float downsamplingFactor)
{
  if (image.header().interleavedComponents()) {
    spdlog::info(
      "Image has multiple, interleaved components, "
      "so the distance map will not be computed");
    return {};
  }

  using TItkImageComp = float;
  using TDistMapComp = uint8_t; // to save GPU memory

  std::vector<DistanceMapImageResult> results;
  results.reserve(image.header().numComponentsPerPixel());

  for (uint32_t comp = 0; comp < image.header().numComponentsPerPixel(); ++comp) {
    const auto compImage = createItkImageFromImageComponent<TItkImageComp>(image, comp);

    const auto thresholds = image.settings().foregroundThresholds(comp);
    const float threshLow = static_cast<float>(thresholds.first);
    const float threshHigh = static_cast<float>(thresholds.second);

    spdlog::debug("Computing Euclidean distance map using thresholds {} and {}", threshLow, threshHigh);

    const auto distMapItkImage = computeEuclideanDistanceMap<TItkImageComp, TDistMapComp>(
      compImage,
      comp,
      threshLow,
      threshHigh,
      std::min(downsamplingFactor, 1.0f));

    if (!distMapItkImage) {
      spdlog::warn("Unable to create distance map for component {}", comp);
      continue;
    }

    const std::string displayName =
      std::string("Dist map for comp ") + std::to_string(comp) + " of '" + image.settings().displayName() + "'";

    results.push_back(DistanceMapImageResult{
      comp,
      createImageFromItkImage<TDistMapComp>(distMapItkImage, displayName),
      thresholds.second});
  }

  return results;
}
