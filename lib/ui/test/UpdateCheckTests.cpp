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
  CHECK_FALSE(compareReleaseVersions("0.9..6", "0.9.6").has_value());
  CHECK_FALSE(compareReleaseVersions("release-0.9.6", "0.9.6").has_value());
  CHECK_FALSE(compareReleaseVersions("", "0.9.6").has_value());
}

TEST_CASE("latest GitHub release JSON rejects non-stable and malformed releases", "[ui][updates]")
{
  std::string error;
  CHECK_FALSE(ui::updates::parseLatestReleaseJson(R"json({"tag_name":"nightly"})json", &error).has_value());
  CHECK_FALSE(error.empty());

  error.clear();
  CHECK_FALSE(
    ui::updates::parseLatestReleaseJson(R"json({"tag_name":"v1.0.0","prerelease":true})json", &error).has_value());
  CHECK_FALSE(error.empty());
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

TEST_CASE("latest GitHub release HTTP response handles current and newer installed versions", "[ui][updates]")
{
  const std::string response =
    "HTTP/2 200\r\n"
    "etag: \"abc\"\r\n"
    "\r\n"
    "{\"tag_name\":\"v0.9.9.0\"}\n";

  CHECK(ui::updates::parseGitHubReleaseHttpResponse(response, "0.9.9.0").status == ui::updates::CheckStatus::UpToDate);
  CHECK(ui::updates::parseGitHubReleaseHttpResponse(response, "0.9.9.2").status == ui::updates::CheckStatus::UpToDate);
  CHECK(
    ui::updates::parseGitHubReleaseHttpResponse(response, "development").status == ui::updates::CheckStatus::Failed);
}

TEST_CASE("latest GitHub release HTTP response uses final redirect metadata", "[ui][updates]")
{
  const std::string response =
    "HTTP/1.1 301 Moved Permanently\r\n"
    "etag: \"redirect\"\r\n"
    "location: https://api.github.com/final\r\n"
    "\r\n"
    "HTTP/2 200\r\n"
    "etag: \"release\"\r\n"
    "\r\n"
    "{\"tag_name\":\"v0.9.9.0\"}\n";

  const auto result = ui::updates::parseGitHubReleaseHttpResponse(response, "0.9.8.0");
  CHECK(result.status == ui::updates::CheckStatus::UpdateAvailable);
  CHECK(result.etag == "\"release\"");
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

TEST_CASE("not-modified responses preserve the cached update decision", "[ui][updates]")
{
  ui::updates::CheckResult notModified;
  notModified.status = ui::updates::CheckStatus::NotModified;
  notModified.etag = "\"new-etag\"";

  ui::updates::CheckResult cached;
  cached.status = ui::updates::CheckStatus::UpdateAvailable;
  cached.etag = "\"old-etag\"";
  cached.latestRelease.tagName = "v0.9.9.0";

  const auto resolved = ui::updates::resolveCachedCheckResult(notModified, cached);
  CHECK(resolved.status == ui::updates::CheckStatus::UpdateAvailable);
  CHECK(resolved.latestRelease.tagName == "v0.9.9.0");
  CHECK(resolved.etag == "\"new-etag\"");

  const auto missingCache = ui::updates::resolveCachedCheckResult(notModified, std::nullopt);
  CHECK(missingCache.status == ui::updates::CheckStatus::Failed);
  CHECK_FALSE(missingCache.error.empty());
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

TEST_CASE("GitHub latest-release endpoint returns a valid Entropy release", "[ui][updates][.network]")
{
  const auto result = ui::updates::fetchLatestRelease({.currentVersion = "0.0.0", .etag = {}});

  INFO(result.error);
  REQUIRE(result.status == ui::updates::CheckStatus::UpdateAvailable);
  CHECK_FALSE(result.latestRelease.tagName.empty());
  CHECK_FALSE(result.latestRelease.htmlUrl.empty());
}
