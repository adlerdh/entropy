#pragma once

#include <optional>
#include <string>

namespace entropy::ui::updates
{
inline constexpr const char* k_downloadPageUrl = "https://github.com/adlerdh/entropy/releases";
inline constexpr const char* k_latestReleaseApiUrl = "https://api.github.com/repos/adlerdh/entropy/releases/latest";

struct ReleaseInfo
{
  std::string tagName;
  std::string name;
  std::string htmlUrl;
  std::string publishedAt;
  std::string body;
};

enum class CheckStatus
{
  UpdateAvailable,
  UpToDate,
  NotModified,
  NoPublishedReleases,
  Failed
};

struct CheckRequest
{
  std::string currentVersion;
  std::string etag;
};

struct CheckResult
{
  CheckStatus status{CheckStatus::Failed};
  ReleaseInfo latestRelease;
  std::string etag;
  std::string error;
};

struct CheckWindowState
{
  bool open = false;
  bool checking = false;
  bool manualCheck = false;
  bool hasResult = false;
  CheckResult result;
};

/**
 * @brief Compare two release version strings.
 *
 * Leading "v" prefixes and non-numeric suffixes are ignored. Missing numeric
 * components are treated as zero, so "v1.2" compares equal to "1.2.0".
 *
 * @return Negative if lhs < rhs, zero if equal, positive if lhs > rhs.
 */
int compareReleaseVersions(std::string lhs, std::string rhs);

/**
 * @brief Parse the JSON returned by GitHub's latest-release endpoint.
 */
std::optional<ReleaseInfo> parseLatestReleaseJson(const std::string& text, std::string* error = nullptr);

/**
 * @brief Parse a curl "-i" HTTP response and extract final response metadata.
 */
CheckResult parseGitHubReleaseHttpResponse(const std::string& responseText, const std::string& currentVersion);

/**
 * @brief Query GitHub's latest-release endpoint using libcurl.
 */
CheckResult fetchLatestRelease(const CheckRequest& request);

/**
 * @brief Open a URL in the user's default browser.
 */
bool openUrlInDefaultBrowser(const std::string& url, std::string* error = nullptr);

/**
 * @brief Render the non-modal update check status window.
 */
void renderUpdateCheckWindow(CheckWindowState& state);

} // namespace entropy::ui::updates
