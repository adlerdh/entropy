#pragma once

#include "common/UuidRange.h"

#include <cstddef>
#include <list>
#include <optional>
#include <vector>

namespace layout
{

/**
 * @brief Convert an optional image UID to its current image-list index.
 * @param orderedImageUids Image UIDs in application order.
 * @param imageUid Image UID to find.
 * @return Image index, or `std::nullopt` when the UID is absent.
 */
std::optional<std::size_t> imageIndexForUid(uuid_range_t orderedImageUids, const std::optional<uuids::uuid>& imageUid);

/**
 * @brief Convert image UIDs to current image-list indices.
 * @param orderedImageUids Image UIDs in application order.
 * @param imageUids Image UIDs to find.
 * @return Indices for UIDs that exist in `orderedImageUids`.
 */
std::vector<std::size_t> imageIndicesForUids(uuid_range_t orderedImageUids, const std::list<uuids::uuid>& imageUids);

/**
 * @brief Convert image indices to image UIDs.
 * @param orderedImageUids Image UIDs in application order.
 * @param imageIndices Image indices to resolve.
 * @return UIDs for indices that are inside `orderedImageUids`.
 */
std::list<uuids::uuid> imageUidsForIndices(uuid_range_t orderedImageUids, const std::vector<std::size_t>& imageIndices);

/**
 * @brief Convert an optional image index to an image UID.
 * @param orderedImageUids Image UIDs in application order.
 * @param imageIndex Image index to resolve.
 * @return Image UID, or `std::nullopt` when the index is absent or out of range.
 */
std::optional<uuids::uuid> imageUidForIndex(
  uuid_range_t orderedImageUids,
  const std::optional<std::size_t>& imageIndex);

} // namespace layout
