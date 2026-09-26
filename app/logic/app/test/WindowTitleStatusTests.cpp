#include "logic/app/WindowTitleStatus.h"

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <optional>
#include <string>

TEST_CASE("Window title uses the complete project filename without its path", "[WindowTitleStatus]")
{
  CHECK(window_title::projectDisplayName("/tmp/project.json") == "project.json");
  CHECK(window_title::projectDisplayName("/tmp/project.JSON") == "project.JSON");
  CHECK(window_title::projectDisplayName("/tmp/project.Json") == "project.Json");
  CHECK(window_title::projectDisplayName("/tmp/project.entropy") == "project.entropy");
}

TEST_CASE("Window title uses image names until the project has a filename", "[WindowTitleStatus]")
{
  CHECK(window_title::status(std::nullopt, "T1", false) == "T1");
  CHECK(window_title::status(std::nullopt, "T1, T2", true) == "T1, T2");
  CHECK(window_title::status(std::filesystem::path{"/tmp/project.json"}, "", false) == "project.json");
  CHECK(window_title::status(std::filesystem::path{"/tmp/project.json"}, "", true) == "project.json*");
  CHECK(window_title::status(std::filesystem::path{"/tmp/project.json"}, "T1, T2", false) == "project.json");
  CHECK(window_title::status(std::filesystem::path{"/tmp/project.json"}, "T1, T2", true) == "project.json*");
}
