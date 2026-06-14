#include "common/UuidUtility.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("random UUID generation returns non-nil unique values", "[common][uuid]")
{
  const uuids::uuid first = generateRandomUuid();
  const uuids::uuid second = generateRandomUuid();

  CHECK(first != uuids::uuid{});
  CHECK(second != uuids::uuid{});
  CHECK(first != second);
}
