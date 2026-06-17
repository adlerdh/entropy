#pragma once

#include "common/UuidRange.h"

#include <uuid.h>

#include <cstddef>
#include <list>
#include <optional>
#include <set>

namespace windowing
{

/**
 * @brief Return the rendered image list after applying default-rendering preferences.
 *
 * @param imageUids Image UIDs in application order.
 * @param defaultRenderAllImages If true, return all image UIDs.
 * @param preferredDefaultRenderedImages Zero-based image positions to keep.
 * @return Ordered rendered-image UIDs.
 */
std::list<uuids::uuid> filteredDefaultRenderedImages(
  const std::list<uuids::uuid>& imageUids,
  bool defaultRenderAllImages,
  const std::set<std::size_t>& preferredDefaultRenderedImages);

/**
 * @brief Reorder an existing selected-image list to match `orderedImageUids`.
 *
 * @param selectedImageUids Current selection.
 * @param orderedImageUids Image UIDs in application order.
 * @param maxCount Optional maximum output size.
 * @return Selected UIDs that still exist, in application order.
 */
std::list<uuids::uuid> reorderSelectedImages(
  const std::list<uuids::uuid>& selectedImageUids,
  uuid_range_t orderedImageUids,
  std::optional<std::size_t> maxCount = std::nullopt);

} // namespace windowing
