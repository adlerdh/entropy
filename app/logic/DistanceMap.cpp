#include "DistanceMap.h"

#include "image/ImageDerivedData.h"

#include <spdlog/fmt/ostr.h>
#include <spdlog/spdlog.h>

void createNoiseEstimates(const Image& image, const uuids::uuid& imageUid, AppData& data)
{
  constexpr uint32_t radius = 1;

  for (auto& result : createNoiseEstimateImages(image, radius)) {
    const glm::uvec3 noiseImgSize = result.image.header().pixelDimensions();

    data.addNoiseEstimate(imageUid, result.component, std::move(result.image), radius);

    spdlog::debug(
      "Created noise estimate ({}x{}x{} voxels) with radius {} for component {} of image {}",
      noiseImgSize.x,
      noiseImgSize.y,
      noiseImgSize.z,
      radius,
      result.component,
      imageUid);
  }
}
