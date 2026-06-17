#include "layout/SyncGroupIndexMap.h"

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <optional>

namespace
{

uuids::uuid uuidFromIndex(uint8_t index)
{
  return uuids::uuid{{index, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}};
}

} // namespace

TEST_CASE("sync group index map assigns stable serialized indices", "[layout][sync_group_index_map]")
{
  layout::SyncGroupIndexMap map;
  const auto firstUid = uuidFromIndex(1);
  const auto secondUid = uuidFromIndex(2);

  CHECK(map.indexForGroupUid(firstUid) == std::optional<std::size_t>{0});
  CHECK(map.indexForGroupUid(secondUid) == std::optional<std::size_t>{1});
  CHECK(map.indexForGroupUid(firstUid) == std::optional<std::size_t>{0});
  CHECK_FALSE(map.indexForGroupUid(std::nullopt));
}

TEST_CASE("sync group index map binds serialized indices to runtime UIDs", "[layout][sync_group_index_map]")
{
  layout::SyncGroupIndexMap map;
  const auto firstUid = uuidFromIndex(1);
  const auto secondUid = uuidFromIndex(2);

  CHECK_FALSE(map.groupUidForIndex(std::size_t{3}));
  CHECK(map.bindGroupIndex(3, firstUid) == firstUid);
  CHECK(map.groupUidForIndex(std::size_t{3}) == std::optional{firstUid});
  CHECK(map.bindGroupIndex(3, secondUid) == firstUid);
  CHECK(map.groupUidForIndex(std::size_t{3}) == std::optional{firstUid});
  CHECK_FALSE(map.groupUidForIndex(std::nullopt));
}
