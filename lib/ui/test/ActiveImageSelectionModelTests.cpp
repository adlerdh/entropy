#include "ui/widgets/ActiveImageSelectionModel.h"

#include <catch2/catch_test_macros.hpp>

namespace active_image_selection = entropy::ui::active_image_selection;

TEST_CASE("active image selection treats an empty image list as empty", "[ui][active-image]")
{
  CHECK(active_image_selection::selectionState(0, 0) == active_image_selection::SelectionState::Empty);
}

TEST_CASE("active image selection accepts in-range indices", "[ui][active-image]")
{
  CHECK(active_image_selection::selectionState(3, 0) == active_image_selection::SelectionState::Valid);
  CHECK(active_image_selection::selectionState(3, 2) == active_image_selection::SelectionState::Valid);
}

TEST_CASE("active image selection rejects out-of-range indices", "[ui][active-image]")
{
  CHECK(active_image_selection::selectionState(3, 3) == active_image_selection::SelectionState::Invalid);
}
