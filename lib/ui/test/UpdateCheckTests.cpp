#include "ui/updates/UpdateCheck.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("release versions compare numeric components", "[ui][updates]")
{
  using ui::updates::compareReleaseVersions;

  CHECK(compareReleaseVersions("0.9.6.0", "v0.9.6") == 0);
  CHECK(compareReleaseVersions("v0.9.7", "0.9.6.0") > 0);
  CHECK(compareReleaseVersions("0.10.0", "0.9.99") > 0);
  CHECK(compareReleaseVersions("1.0.0-beta", "1.0.0") == 0);
  CHECK(compareReleaseVersions("0.9.5", "0.9.6") < 0);
}

TEST_CASE("latest GitHub release JSON is parsed", "[ui][updates]")
{
  const std::string text = R"json({
    "tag_name": "v0.9.7",
    "name": "Entropy 0.9.7",
    "html_url": "https://github.com/adlerdh/entropy/releases/tag/v0.9.7",
    "published_at": "2026-07-04T12:00:00Z",
    "body": "Release notes"
  })json";

  std::string error;
  const auto release = ui::updates::parseLatestReleaseJson(text, &error);
  REQUIRE(release.has_value());
  CHECK(release->tagName == "v0.9.7");
  CHECK(release->name == "Entropy 0.9.7");
  CHECK(release->htmlUrl == "https://github.com/adlerdh/entropy/releases/tag/v0.9.7");
  CHECK(error.empty());
}

TEST_CASE("latest GitHub release HTTP response detects update", "[ui][updates]")
{
  const std::string response =
    "HTTP/2 200\r\n"
    "etag: \"abc\"\r\n"
    "\r\n"
    "{\"tag_name\":\"v0.9.7\",\"html_url\":\"https://github.com/adlerdh/entropy/releases/tag/v0.9.7\"}\n";

  const auto result = ui::updates::parseGitHubReleaseHttpResponse(response, "0.9.6.0");
  CHECK(result.status == ui::updates::CheckStatus::UpdateAvailable);
  CHECK(result.etag == "\"abc\"");
  CHECK(result.latestRelease.tagName == "v0.9.7");
}

TEST_CASE("latest GitHub release HTTP response handles not modified", "[ui][updates]")
{
  const std::string response =
    "HTTP/2 304\r\n"
    "etag: \"abc\"\r\n"
    "\r\n";

  const auto result = ui::updates::parseGitHubReleaseHttpResponse(response, "0.9.6.0");
  CHECK(result.status == ui::updates::CheckStatus::NotModified);
  CHECK(result.etag == "\"abc\"");
}

TEST_CASE("latest GitHub release HTTP response handles missing releases", "[ui][updates]")
{
  const std::string response =
    "HTTP/2 404\r\n"
    "content-type: application/json; charset=utf-8\r\n"
    "\r\n"
    R"json({"message":"Not Found","status":"404"})json"
    "\n";

  const auto result = ui::updates::parseGitHubReleaseHttpResponse(response, "0.9.6.0");
  CHECK(result.status == ui::updates::CheckStatus::NoPublishedReleases);
}
