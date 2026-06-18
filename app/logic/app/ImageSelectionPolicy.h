#pragma once

#include "common/UuidRange.h"

#include <uuid.h>

#include <cstddef>
#include <list>
#include <optional>

namespace app::image_selection_policy
{

/**
 * @brief Choose the active image index for a new project load.
 *
 * @param numImages Number of loaded images in application order.
 * @return Last loaded image index, or `std::nullopt` when no image is loaded.
 */
std::optional<std::size_t> defaultActiveImageIndexForInitialLoad(std::size_t numImages);

/**
 * @brief Choose default metric images from reference and active-image state.
 *
 * @param orderedImageUids Loaded image UIDs in application order.
 * @param referenceImageUid Reference image UID, if assigned.
 * @param activeImageUid Active image UID, if assigned.
 * @return Up to two image UIDs. The reference image is first when valid, followed
 * by the active image when valid and distinct. Missing choices fall back to the
 * first loaded images.
 */
std::list<uuids::uuid> defaultMetricImageUids(
  uuid_range_t orderedImageUids,
  const std::optional<uuids::uuid>& referenceImageUid,
  const std::optional<uuids::uuid>& activeImageUid);

} // namespace app::image_selection_policy
