#include "ui/windows/RegionStatisticsController.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>

TEST_CASE("Region statistics controller optionally adds empty label rows", "[ui][statistics][controller]")
{
  RegionStatisticsController controller;
  controller.result =
    RegionStatisticsResult{.regions = {{.label = 1, .elementCount = 3, .finiteValueCount = 3, .physicalSize = 6.0}}};
  controller.sortedRegions = controller.result->regions;
  controller.includeEmptyLabels = true;

  CHECK(controller.synchronizeEmptyLabelRows(4));
  REQUIRE(controller.sortedRegions.size() == 4);
  const auto emptyLabel =
    std::find_if(controller.sortedRegions.begin(), controller.sortedRegions.end(), [](const RegionStatistic& region) {
      return 2 == region.label;
    });
  REQUIRE(emptyLabel != controller.sortedRegions.end());
  CHECK(emptyLabel->elementCount == 0);
  CHECK(emptyLabel->physicalSize == 0.0);
  CHECK_FALSE(emptyLabel->median);

  controller.includeEmptyLabels = false;
  CHECK(controller.synchronizeEmptyLabelRows(4));
  REQUIRE(controller.sortedRegions.size() == 1);
  CHECK(controller.sortedRegions.front().label == 1);
}
