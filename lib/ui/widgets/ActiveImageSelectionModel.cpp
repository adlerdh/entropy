#include "ui/widgets/ActiveImageSelectionModel.h"

namespace entropy::ui::active_image_selection
{

SelectionState selectionState(std::size_t numImages, std::size_t activeIndex)
{
  if (0 == numImages) {
    return SelectionState::Empty;
  }

  return activeIndex < numImages ? SelectionState::Valid : SelectionState::Invalid;
}

} // namespace entropy::ui::active_image_selection
