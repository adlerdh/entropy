#include "logic/app/WindowTitleStatus.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("Window title strips project JSON extension case-insensitively", "[WindowTitleStatus]")
{
  CHECK(window_title::projectDisplayName("/tmp/project.json") == "project");
  CHECK(window_title::projectDisplayName("/tmp/project.JSON") == "project");
  CHECK(window_title::projectDisplayName("/tmp/project.Json") == "project");
  CHECK(window_title::projectDisplayName("/tmp/project.entropy") == "project.entropy");
}

TEST_CASE("Window title status includes project, image names, and dirty marker", "[WindowTitleStatus]")
{
  CHECK(window_title::status(std::nullopt, "T1", false) == "T1");
  CHECK(window_title::status(std::filesystem::path{"/tmp/project.json"}, "", false) == "project");
  CHECK(window_title::status(std::filesystem::path{"/tmp/project.json"}, "", true) == "project*");
  CHECK(window_title::status(std::filesystem::path{"/tmp/project.json"}, "T1, T2", false) == "project: T1, T2");
  CHECK(window_title::status(std::filesystem::path{"/tmp/project.json"}, "T1, T2", true) == "project*: T1, T2");
}
