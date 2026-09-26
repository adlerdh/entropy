#include "ui/windows/ViewOverlayModel.h"
#include "viewer/ViewModes.h"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <string>
#include <string_view>

namespace view_overlay = ui::view_overlay;

TEST_CASE("view overlay chooses image or metric selection by render mode", "[ui][view_overlay]")
{
  CHECK(view_overlay::usesVisibleImageSelection(ViewRenderMode::Image));

  CHECK(view_overlay::usesMetricImageSelection(ViewRenderMode::Overlay));
  CHECK(view_overlay::usesMetricImageSelection(ViewRenderMode::Difference));
  CHECK(view_overlay::usesMetricImageSelection(ViewRenderMode::LocalNcc));
  CHECK(view_overlay::usesMetricImageSelection(ViewRenderMode::LocalLinearResidual));
  CHECK(view_overlay::usesMetricImageSelection(ViewRenderMode::JointHistogram));

  CHECK_FALSE(view_overlay::usesMetricImageSelection(ViewRenderMode::Image));
  CHECK_FALSE(view_overlay::usesMetricImageSelection(ViewRenderMode::Disabled));

  CHECK(view_overlay::usesDisabledVisibilityIcon(ViewRenderMode::Disabled));
  CHECK_FALSE(view_overlay::usesDisabledVisibilityIcon(ViewRenderMode::Image));

  CHECK(view_overlay::usesAnatomicalViewTypeSelector(ViewRenderMode::Image));
  CHECK(view_overlay::usesAnatomicalViewTypeSelector(ViewRenderMode::Difference));
  CHECK_FALSE(view_overlay::usesAnatomicalViewTypeSelector(ViewRenderMode::JointHistogram));
}

TEST_CASE("view overlay labels image choices with visibility and active state", "[ui][view_overlay]")
{
  CHECK(view_overlay::imageChoiceLabel({"T1", true, false, false}) == "T1");
  CHECK(view_overlay::imageChoiceLabel({"T2", false, false, false}) == "T2 (hidden)");
  CHECK(view_overlay::imageChoiceLabel({"FLAIR", true, true, false}) == "FLAIR (active)");
  CHECK(view_overlay::imageChoiceLabel({"T1", true, false, true}) == "T1 (ref)");
  CHECK(view_overlay::imageChoiceLabel({"Seg", false, true, true}) == "Seg (hidden) (ref + active)");
}

TEST_CASE("image choice role badges are compact and consistently ordered", "[ui][view_overlay]")
{
  using view_overlay::ImageChoice;
  using view_overlay::imageChoiceRoleBadges;

  CHECK((imageChoiceRoleBadges(ImageChoice{}) == std::array<std::string_view, 2>{"", ""}));
  CHECK((imageChoiceRoleBadges(ImageChoice{"", true, false, true}) == std::array<std::string_view, 2>{"REF", ""}));
  CHECK((imageChoiceRoleBadges(ImageChoice{"", true, true, false}) == std::array<std::string_view, 2>{"", "ACT"}));
  CHECK((imageChoiceRoleBadges(ImageChoice{"", true, true, true}) == std::array<std::string_view, 2>{"REF", "ACT"}));
}
