#include "ui/windows/ViewOverlayModel.h"

#include <catch2/catch_test_macros.hpp>

namespace view_overlay = entropy::ui::view_overlay;

TEST_CASE("view overlay chooses image or metric selection by render mode", "[ui][view_overlay]")
{
  CHECK(view_overlay::usesVisibleImageSelection(ViewRenderMode::Image));
  CHECK(view_overlay::usesVisibleImageSelection(ViewRenderMode::VolumeRender));

  CHECK(view_overlay::usesMetricImageSelection(ViewRenderMode::Overlay));
  CHECK(view_overlay::usesMetricImageSelection(ViewRenderMode::Difference));
  CHECK(view_overlay::usesMetricImageSelection(ViewRenderMode::LocalNcc));
  CHECK(view_overlay::usesMetricImageSelection(ViewRenderMode::LocalLinearResidual));
  CHECK(view_overlay::usesMetricImageSelection(ViewRenderMode::JointHistogram));

  CHECK_FALSE(view_overlay::usesMetricImageSelection(ViewRenderMode::Image));
  CHECK_FALSE(view_overlay::usesMetricImageSelection(ViewRenderMode::Disabled));

  CHECK(view_overlay::usesDisabledVisibilityIcon(ViewRenderMode::Disabled));
  CHECK_FALSE(view_overlay::usesDisabledVisibilityIcon(ViewRenderMode::Image));
}

TEST_CASE("view overlay labels image choices with visibility and active state", "[ui][view_overlay]")
{
  CHECK(view_overlay::imageChoiceLabel({"T1", true, false, false, false}) == "T1");
  CHECK(view_overlay::imageChoiceLabel({"T2", false, false, false, false}) == "T2 (hidden)");
  CHECK(view_overlay::imageChoiceLabel({"FLAIR", true, true, false, false}) == "FLAIR (active)");
  CHECK(view_overlay::imageChoiceLabel({"T1", true, false, true, false}) == "T1 (ref.)");
  CHECK(view_overlay::imageChoiceLabel({"Seg", false, true, true, false}) == "Seg (hidden) (active) (ref.)");
}

TEST_CASE("view overlay joins selected visible image names", "[ui][view_overlay]")
{
  const std::vector<view_overlay::ImageChoice> choices{
    {"T1", true, true, true, true},
    {"T2", false, false, false, true},
    {"FLAIR", true, false, false, false},
    {"CT", true, false, false, true}};

  CHECK(view_overlay::selectedVisibleImageNames(choices) == "T1 (active) (ref.), CT");
}
