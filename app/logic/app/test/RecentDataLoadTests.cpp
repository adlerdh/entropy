#include "logic/app/RecentDataLoad.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("failed loads do not produce Recent data entries")
{
  recent_data::PendingLoad pending;
  pending.begin(recent_data::Kind::Project, {"failed.entropy.json"});
  pending.cancel();

  CHECK_FALSE(pending.takeCompleted());
  CHECK_FALSE(pending.kind());
}

TEST_CASE("completed loads produce their deferred Recent data entry once")
{
  recent_data::PendingLoad pending;
  pending.begin(recent_data::Kind::Dicom, {"dicom-folder"});

  const auto completed = pending.takeCompleted();
  REQUIRE(completed);
  CHECK(completed->kind == recent_data::Kind::Dicom);
  CHECK(completed->paths == std::vector<std::filesystem::path>{"dicom-folder"});
  CHECK_FALSE(pending.takeCompleted());
}

TEST_CASE("partial image loads retain only successful paths")
{
  recent_data::PendingLoad pending;
  pending.begin(recent_data::Kind::Images, {});
  pending.replacePaths({"loaded-a.nii.gz", "loaded-c.nii.gz"});

  const auto completed = pending.takeCompleted();
  REQUIRE(completed);
  CHECK(completed->kind == recent_data::Kind::Images);
  CHECK(completed->paths == std::vector<std::filesystem::path>{"loaded-a.nii.gz", "loaded-c.nii.gz"});
}

TEST_CASE("loads with no successful paths do not produce Recent data entries")
{
  recent_data::PendingLoad pending;
  pending.begin(recent_data::Kind::Images, {});

  CHECK_FALSE(pending.takeCompleted());
  CHECK_FALSE(pending.kind());
}
