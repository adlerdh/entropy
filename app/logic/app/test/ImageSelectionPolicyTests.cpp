#include "logic/app/ImageSelectionPolicy.h"

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <list>
#include <optional>

namespace
{

uuids::uuid testUid(std::uint8_t index)
{
  return uuids::uuid{{index, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}};
}

} // namespace

TEST_CASE("initial load active image chooses the last loaded image", "[app][image_selection_policy]")
{
  namespace policy = app::image_selection_policy;

  CHECK_FALSE(policy::defaultActiveImageIndexForInitialLoad(0).has_value());
  CHECK(policy::defaultActiveImageIndexForInitialLoad(1) == 0);
  CHECK(policy::defaultActiveImageIndexForInitialLoad(2) == 1);
  CHECK(policy::defaultActiveImageIndexForInitialLoad(5) == 4);
}

TEST_CASE("default metric images use reference and active images", "[app][image_selection_policy]")
{
  namespace policy = app::image_selection_policy;

  const uuid_range_t imageUids{testUid(1), testUid(2), testUid(3)};

  const std::list<uuids::uuid> metricImages = policy::defaultMetricImageUids(imageUids, testUid(1), testUid(3));

  REQUIRE(metricImages.size() == 2);
  CHECK(metricImages.front() == testUid(1));
  CHECK(metricImages.back() == testUid(3));
}

TEST_CASE("default metric images fill missing choices from image order", "[app][image_selection_policy]")
{
  namespace policy = app::image_selection_policy;

  const uuid_range_t imageUids{testUid(1), testUid(2), testUid(3)};

  CHECK(policy::defaultMetricImageUids(imageUids, std::nullopt, testUid(3)) == std::list{testUid(1), testUid(2)});
  CHECK(policy::defaultMetricImageUids(imageUids, testUid(1), testUid(1)) == std::list{testUid(1), testUid(2)});
  CHECK(policy::defaultMetricImageUids(imageUids, testUid(3), std::nullopt) == std::list{testUid(3), testUid(1)});
  CHECK(policy::defaultMetricImageUids(uuid_range_t{testUid(1)}, testUid(1), std::nullopt) == std::list{testUid(1)});
}
