#include "viewer/ImageSelection.h"

#include <algorithm>

namespace viewer::image_selection
{

std::list<uuids::uuid> filteredDefaultRenderedImages(
  const std::list<uuids::uuid>& imageUids,
  bool defaultRenderAllImages,
  const std::set<std::size_t>& preferredDefaultRenderedImages)
{
  if (defaultRenderAllImages) {
    return imageUids;
  }

  std::list<uuids::uuid> filteredImageUids;
  std::size_t index = 0;

  for (const auto& imageUid : imageUids) {
    if (preferredDefaultRenderedImages.count(index) > 0) {
      filteredImageUids.push_back(imageUid);
    }
    ++index;
  }

  return filteredImageUids;
}

std::list<uuids::uuid> reorderSelectedImages(
  const std::list<uuids::uuid>& selectedImageUids,
  uuid_range_t orderedImageUids,
  std::optional<std::size_t> maxCount)
{
  std::list<uuids::uuid> reorderedImageUids;

  for (const auto& imageUid : orderedImageUids) {
    if (maxCount && reorderedImageUids.size() >= *maxCount) {
      break;
    }

    if (std::find(selectedImageUids.begin(), selectedImageUids.end(), imageUid) != selectedImageUids.end()) {
      reorderedImageUids.push_back(imageUid);
    }
  }

  return reorderedImageUids;
}

} // namespace viewer::image_selection
