#include "layout/ImageIndexMapping.h"

#include <algorithm>

namespace layout
{

std::optional<std::size_t> imageIndexForUid(uuid_range_t orderedImageUids, const std::optional<uuids::uuid>& imageUid)
{
  if (!imageUid) {
    return std::nullopt;
  }

  const auto it = std::find(orderedImageUids.begin(), orderedImageUids.end(), *imageUid);
  if (it == orderedImageUids.end()) {
    return std::nullopt;
  }
  return static_cast<std::size_t>(std::distance(orderedImageUids.begin(), it));
}

std::vector<std::size_t> imageIndicesForUids(uuid_range_t orderedImageUids, const std::list<uuids::uuid>& imageUids)
{
  std::vector<std::size_t> indices;
  for (const auto& imageUid : imageUids) {
    const auto it = std::find(orderedImageUids.begin(), orderedImageUids.end(), imageUid);
    if (it != orderedImageUids.end()) {
      indices.push_back(static_cast<std::size_t>(std::distance(orderedImageUids.begin(), it)));
    }
  }
  return indices;
}

std::list<uuids::uuid> imageUidsForIndices(uuid_range_t orderedImageUids, const std::vector<std::size_t>& imageIndices)
{
  std::list<uuids::uuid> imageUids;
  for (const std::size_t imageIndex : imageIndices) {
    if (imageIndex < orderedImageUids.size()) {
      imageUids.push_back(orderedImageUids.at(imageIndex));
    }
  }
  return imageUids;
}

std::optional<uuids::uuid> imageUidForIndex(uuid_range_t orderedImageUids, const std::optional<std::size_t>& imageIndex)
{
  if (!imageIndex || *imageIndex >= orderedImageUids.size()) {
    return std::nullopt;
  }
  return orderedImageUids.at(*imageIndex);
}

} // namespace layout
