#include "layout/ImageIndexMapping.h"

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <optional>
#include <vector>

namespace
{

uuids::uuid uuidFromIndex(uint8_t index)
{
  return uuids::uuid{{index, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}};
}

} // namespace

TEST_CASE("image index mapping resolves optional image UIDs", "[layout][image_index_mapping]")
{
  const std::vector<uuids::uuid> imageUids{uuidFromIndex(1), uuidFromIndex(2), uuidFromIndex(3)};

  CHECK(layout::imageIndexForUid(imageUids, uuidFromIndex(2)) == std::optional<std::size_t>{1});
  CHECK_FALSE(layout::imageIndexForUid(imageUids, std::nullopt));
  CHECK_FALSE(layout::imageIndexForUid(imageUids, uuidFromIndex(9)));
}

TEST_CASE("image index mapping skips missing UIDs and out-of-range indices", "[layout][image_index_mapping]")
{
  const std::vector<uuids::uuid> imageUids{uuidFromIndex(1), uuidFromIndex(2), uuidFromIndex(3)};

  CHECK(
    layout::imageIndicesForUids(imageUids, {uuidFromIndex(3), uuidFromIndex(9), uuidFromIndex(1)}) ==
    std::vector<std::size_t>{2, 0});
  CHECK(
    layout::imageUidsForIndices(imageUids, {1, 8, 0}) == std::list<uuids::uuid>{uuidFromIndex(2), uuidFromIndex(1)});
}

TEST_CASE("image index mapping resolves optional image indices", "[layout][image_index_mapping]")
{
  const std::vector<uuids::uuid> imageUids{uuidFromIndex(1), uuidFromIndex(2)};

  CHECK(layout::imageUidForIndex(imageUids, std::size_t{0}) == std::optional{uuidFromIndex(1)});
  CHECK_FALSE(layout::imageUidForIndex(imageUids, std::nullopt));
  CHECK_FALSE(layout::imageUidForIndex(imageUids, std::size_t{4}));
}
