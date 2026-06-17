#pragma once

#include "common/UuidRange.h"

#include <uuid.h>

#include <cstddef>
#include <list>
#include <optional>
#include <set>

namespace viewer_types::image_selection
{

/**
 * @brief Apply default rendered-image preferences to an ordered image list.
 *
 * @param imageUids Image UIDs in application order.
 * @param defaultRenderAllImages If true, every image is kept.
 * @param preferredDefaultRenderedImages Zero-based image positions to keep when render-all is false.
 * @return Ordered image UIDs selected for default rendering.
 */
std::list<uuids::uuid> filteredDefaultRenderedImages(
  const std::list<uuids::uuid>& imageUids,
  bool defaultRenderAllImages,
  const std::set<std::size_t>& preferredDefaultRenderedImages);

/**
 * @brief Reorder a selection to match the current image order.
 *
 * @param selectedImageUids Current selected image UIDs.
 * @param orderedImageUids Image UIDs in application order.
 * @param maxCount Optional maximum number of selected UIDs to keep.
 * @return Selected UIDs that still exist, in application order.
 */
std::list<uuids::uuid> reorderSelectedImages(
  const std::list<uuids::uuid>& selectedImageUids,
  uuid_range_t orderedImageUids,
  std::optional<std::size_t> maxCount = std::nullopt);

} // namespace viewer_types::image_selection
