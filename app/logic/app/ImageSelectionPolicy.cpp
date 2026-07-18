#include "logic/app/ImageSelectionPolicy.h"

#include <algorithm>

namespace app::image_selection_policy
{
namespace
{

bool contains(uuid_range_t imageUids, const uuids::uuid& imageUid)
{
  return std::find(imageUids.begin(), imageUids.end(), imageUid) != imageUids.end();
}

bool selected(const std::list<uuids::uuid>& imageUids, const uuids::uuid& imageUid)
{
  return std::find(imageUids.begin(), imageUids.end(), imageUid) != imageUids.end();
}

} // namespace

std::optional<std::size_t> defaultActiveImageIndexForInitialLoad(std::size_t numImages)
{
  if (0 == numImages) {
    return std::nullopt;
  }
  return numImages - 1;
}

std::list<uuids::uuid> defaultMetricImageUids(
  const uuid_range_t& orderedImageUids,
  const std::optional<uuids::uuid>& referenceImageUid,
  const std::optional<uuids::uuid>& activeImageUid)
{
  static constexpr std::size_t sk_maxMetricImages = 2;

  std::list<uuids::uuid> metricImageUids;

  if (referenceImageUid && contains(orderedImageUids, *referenceImageUid)) {
    metricImageUids.push_back(*referenceImageUid);

    if (activeImageUid && *activeImageUid != *referenceImageUid && contains(orderedImageUids, *activeImageUid)) {
      metricImageUids.push_back(*activeImageUid);
    }
  }

  for (const auto& imageUid : orderedImageUids) {
    if (metricImageUids.size() >= sk_maxMetricImages) {
      break;
    }
    if (!selected(metricImageUids, imageUid)) {
      metricImageUids.push_back(imageUid);
    }
  }

  return metricImageUids;
}

} // namespace app::image_selection_policy
