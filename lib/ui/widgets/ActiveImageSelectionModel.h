#pragma once

#include <cstddef>

namespace ui::active_image_selection
{

enum class SelectionState
{
  Empty,  //!< There are no images to select
  Valid,  //!< The active index names an available image
  Invalid //!< Images exist, but the active index is out of range
};

/**
 * @brief Classify whether an active image index can be used by an image selector.
 * @param numImages Number of images available to select.
 * @param activeIndex Current active image index.
 * @return Empty when there are no images, Valid when the index is usable, otherwise Invalid.
 */
SelectionState selectionState(std::size_t numImages, std::size_t activeIndex);

} // namespace ui::active_image_selection
