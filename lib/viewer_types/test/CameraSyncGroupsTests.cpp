#include "viewer_types/CameraSyncGroups.h"

#include <catch2/catch_test_macros.hpp>

#include <uuid.h>

#include <cstdint>
#include <optional>

namespace
{

uuids::uuid uuidFromIndex(uint8_t index)
{
  return uuids::uuid{{index, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}};
}

} // namespace

TEST_CASE("camera sync groups create independent groups by mode", "[viewer_types][camera_sync_groups]")
{
  viewer_types::CameraSyncGroups groups;

  const auto rotationGroup = groups.addGroup(CameraSyncMode::Rotation);
  const auto zoomGroup = groups.addGroup(CameraSyncMode::Zoom);

  REQUIRE(groups.group(CameraSyncMode::Rotation, rotationGroup) != nullptr);
  REQUIRE(groups.group(CameraSyncMode::Zoom, zoomGroup) != nullptr);
  CHECK(groups.group(CameraSyncMode::Zoom, rotationGroup) == nullptr);
}

TEST_CASE("camera sync groups track view membership", "[viewer_types][camera_sync_groups]")
{
  viewer_types::CameraSyncGroups groups;
  const auto groupUid = groups.addGroup(CameraSyncMode::Translation);
  const auto firstView = uuidFromIndex(1);
  const auto secondView = uuidFromIndex(2);

  groups.addViewToGroup(CameraSyncMode::Translation, groupUid, firstView);
  groups.addViewToGroup(CameraSyncMode::Translation, groupUid, secondView);

  const auto* group = groups.group(CameraSyncMode::Translation, groupUid);
  REQUIRE(group != nullptr);
  CHECK(*group == viewer_types::CameraSyncGroups::Group{firstView, secondView});
  CHECK(groups.groupUidContainingView(CameraSyncMode::Translation, firstView) == std::optional{groupUid});
  CHECK_FALSE(groups.groupUidContainingView(CameraSyncMode::Translation, uuidFromIndex(3)));
}

TEST_CASE("camera sync groups ignore missing optional group UIDs", "[viewer_types][camera_sync_groups]")
{
  viewer_types::CameraSyncGroups groups;
  const auto groupUid = groups.addGroup(CameraSyncMode::Rotation);
  const auto viewUid = uuidFromIndex(1);

  groups.addViewToGroup(CameraSyncMode::Rotation, std::nullopt, viewUid);
  groups.addViewToGroup(CameraSyncMode::Rotation, uuidFromIndex(9), viewUid);

  const auto* group = groups.group(CameraSyncMode::Rotation, groupUid);
  REQUIRE(group != nullptr);
  CHECK(group->empty());
}
