#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#endif

#include "ui/updates/UpdateCheck.h"

#include "registration/Process.h"

#include <curl/curl.h>
#include <imgui/imgui.h>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <cfloat>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <memory>
#include <sstream>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#ifdef _WIN32
#include <Windows.h>
#include <shellapi.h>
#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif
#endif

#ifndef ENTROPY_VERSION
#define ENTROPY_VERSION "0.0.0"
#endif

namespace ui::updates
{
namespace
{
CheckResult failedResult(std::string error)
{
  CheckResult result;
  result.status = CheckStatus::Failed;
  result.error = std::move(error);
  return result;
}

std::string trim(std::string value)
{
  const auto isSpace = [](unsigned char c) {
    return std::isspace(c) != 0;
  };
  value.erase(value.begin(), std::find_if(value.begin(), value.end(), [&](char c) { return !isSpace(c); }));
  value.erase(std::find_if(value.rbegin(), value.rend(), [&](char c) { return !isSpace(c); }).base(), value.end());
  return value;
}

std::string toLower(std::string value)
{
  std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  return value;
}

std::optional<std::vector<std::uint64_t>> versionParts(std::string value)
{
  value = trim(std::move(value));
  if (!value.empty() && (value.front() == 'v' || value.front() == 'V')) {
    value.erase(value.begin());
  }

  const std::size_t suffix = value.find_first_of("-+");
  const std::size_t numericLength = suffix == std::string::npos ? value.size() : suffix;
  const std::string_view numericPart{value.data(), numericLength};
  if (numericPart.empty() || (suffix != std::string::npos && suffix + 1 == value.size())) {
    return std::nullopt;
  }

  std::vector<std::uint64_t> parts;
  std::size_t begin = 0;
  while (begin < numericPart.size()) {
    const std::size_t end = numericPart.find('.', begin);
    const std::string_view component = numericPart.substr(begin, end - begin);
    if (component.empty()) {
      return std::nullopt;
    }

    std::uint64_t part = 0;
    const auto parseResult = std::from_chars(component.data(), component.data() + component.size(), part);
    if (parseResult.ec != std::errc{} || parseResult.ptr != component.data() + component.size()) {
      return std::nullopt;
    }
    parts.push_back(part);

    if (end == std::string_view::npos) {
      break;
    }
    begin = end + 1;
  }

  if (numericPart.back() == '.') {
    return std::nullopt;
  }
  return parts;
}

CheckResult resultForRelease(std::string currentVersion, ReleaseInfo release, std::string etag)
{
  CheckResult result;
  result.etag = std::move(etag);
  result.latestRelease = std::move(release);

  const std::optional<int> comparison = compareReleaseVersions(currentVersion, result.latestRelease.tagName);
  if (!comparison) {
    result.status = CheckStatus::Failed;
    result.error = "Could not compare installed version '" + currentVersion + "' with GitHub release tag '" +
                   result.latestRelease.tagName + "'";
    return result;
  }

  result.status = *comparison < 0 ? CheckStatus::UpdateAvailable : CheckStatus::UpToDate;
  return result;
}

int parseHttpStatus(std::string_view line)
{
  constexpr std::string_view prefix = "HTTP/";
  if (!line.starts_with(prefix)) {
    return 0;
  }

  const std::size_t firstSpace = line.find(' ');
  if (firstSpace == std::string_view::npos) {
    return 0;
  }

  const std::size_t secondSpace = line.find(' ', firstSpace + 1);
  const std::string_view statusText = line.substr(
    firstSpace + 1,
    secondSpace == std::string_view::npos ? std::string_view::npos : secondSpace - firstSpace - 1);
  int status = 0;
  static_cast<void>(std::from_chars(statusText.data(), statusText.data() + statusText.size(), status));
  return status;
}

struct CurlGlobalState
{
  CurlGlobalState() : code(curl_global_init(CURL_GLOBAL_DEFAULT)) {}

  ~CurlGlobalState()
  {
    if (code == CURLE_OK) {
      curl_global_cleanup();
    }
  }

  CURLcode code = CURLE_FAILED_INIT;
};

const CurlGlobalState& curlGlobalState()
{
  static const CurlGlobalState state;
  return state;
}

struct CurlEasyDeleter
{
  void operator()(CURL* curl) const
  {
    if (curl != nullptr) {
      curl_easy_cleanup(curl);
    }
  }
};

struct CurlHeaderListDeleter
{
  void operator()(curl_slist* headers) const
  {
    if (headers != nullptr) {
      curl_slist_free_all(headers);
    }
  }
};

using CurlEasyHandle = std::unique_ptr<CURL, CurlEasyDeleter>;
using CurlHeaderList = std::unique_ptr<curl_slist, CurlHeaderListDeleter>;

struct CurlResponse
{
  std::string body;
  std::string etag;
};

bool appendCurlHeader(curl_slist*& headers, const std::string& header)
{
  curl_slist* appendedHeaders = curl_slist_append(headers, header.c_str());
  if (appendedHeaders == nullptr) {
    return false;
  }
  headers = appendedHeaders;
  return true;
}

#if defined(__linux__)
bool hasNonEmptyEnvironmentVariable(const char* name)
{
  const char* value = std::getenv(name);
  return value != nullptr && value[0] != '\0';
}

void configureLinuxCaStore(CURL* curl)
{
  namespace fs = std::filesystem;

  if (!hasNonEmptyEnvironmentVariable("CURL_CA_BUNDLE") && !hasNonEmptyEnvironmentVariable("SSL_CERT_FILE")) {
    static constexpr std::array k_caBundleFiles{
      "/etc/ssl/certs/ca-certificates.crt",
      "/etc/pki/tls/certs/ca-bundle.crt",
      "/etc/pki/ca-trust/extracted/pem/tls-ca-bundle.pem",
      "/etc/ssl/ca-bundle.pem"};

    for (const char* path : k_caBundleFiles) {
      std::error_code error;
      if (fs::is_regular_file(path, error)) {
        curl_easy_setopt(curl, CURLOPT_CAINFO, path);
        break;
      }
    }
  }

  if (!hasNonEmptyEnvironmentVariable("SSL_CERT_DIR")) {
    static constexpr std::array k_caCertificateDirs{"/etc/ssl/certs", "/etc/pki/tls/certs"};

    for (const char* path : k_caCertificateDirs) {
      std::error_code error;
      if (fs::is_directory(path, error)) {
        curl_easy_setopt(curl, CURLOPT_CAPATH, path);
        break;
      }
    }
  }
}
#endif

std::size_t writeCurlBody(char* ptr, std::size_t size, std::size_t nmemb, void* userdata)
{
  const std::size_t byteCount = size * nmemb;
  auto* response = static_cast<CurlResponse*>(userdata);
  response->body.append(ptr, byteCount);
  return byteCount;
}

std::size_t writeCurlHeader(char* ptr, std::size_t size, std::size_t nmemb, void* userdata)
{
  const std::size_t byteCount = size * nmemb;
  auto* response = static_cast<CurlResponse*>(userdata);
  std::string_view line{ptr, byteCount};
  if (!line.empty() && line.back() == '\n') {
    line.remove_suffix(1);
  }
  if (!line.empty() && line.back() == '\r') {
    line.remove_suffix(1);
  }

  // libcurl invokes this callback for every response in a redirect or proxy
  // chain. Only retain metadata from the final response.
  if (line.starts_with("HTTP/")) {
    response->etag.clear();
    return byteCount;
  }

  constexpr std::string_view etagPrefix = "etag:";
  if (line.size() >= etagPrefix.size()) {
    std::string name{line.substr(0, etagPrefix.size())};
    name = toLower(name);
    if (name == etagPrefix) {
      response->etag = trim(std::string{line.substr(etagPrefix.size())});
    }
  }
  return byteCount;
}

void renderWrappedLabel(const char* text)
{
  ImGui::TextWrapped("%s", text);
}

float textWidthWithPadding(const char* text)
{
  const ImGuiStyle& style = ImGui::GetStyle();
  return ImGui::CalcTextSize(text).x + 2.0f * style.WindowPadding.x;
}

float updateCheckWindowWidth(const CheckWindowState& state)
{
  constexpr float minWidth = 360.0f;
  constexpr float maxWidth = 620.0f;

  float width = minWidth;
  const auto includeText = [&](const char* text) {
    width = std::max(width, textWidthWithPadding(text));
  };
  const auto includeString = [&](const std::string& text) {
    if (!text.empty()) {
      width = std::max(width, textWidthWithPadding(text.c_str()));
    }
  };

  includeText("Open Download Page");
  includeText("Checking GitHub for Entropy updates...");

  if (state.checking) {
    includeText("Checking GitHub for Entropy updates...");
  }
  else if (!state.hasResult) {
    includeText("No update check has been run yet.");
  }
  else {
    switch (state.result.status) {
      case CheckStatus::UpdateAvailable:
        includeText("An Entropy update is available.");
        includeString(state.result.latestRelease.tagName);
        includeString(state.result.latestRelease.name);
        break;
      case CheckStatus::UpToDate:
      case CheckStatus::NotModified:
        includeText("Entropy is up to date.");
        break;
      case CheckStatus::NoPublishedReleases:
        includeText("No published Entropy releases were found on GitHub yet.");
        break;
      case CheckStatus::Failed:
        includeText("Could not check for updates.");
        includeString(state.result.error);
        break;
    }
  }

  return std::clamp(width, minWidth, maxWidth);
}
} // namespace

std::optional<int> compareReleaseVersions(std::string lhs, std::string rhs)
{
  std::optional<std::vector<std::uint64_t>> lhsParts = versionParts(std::move(lhs));
  std::optional<std::vector<std::uint64_t>> rhsParts = versionParts(std::move(rhs));
  if (!lhsParts || !rhsParts) {
    return std::nullopt;
  }

  const std::size_t count = std::max(lhsParts->size(), rhsParts->size());
  lhsParts->resize(count, 0);
  rhsParts->resize(count, 0);

  for (std::size_t i = 0; i < count; ++i) {
    if ((*lhsParts)[i] < (*rhsParts)[i]) {
      return -1;
    }
    if ((*lhsParts)[i] > (*rhsParts)[i]) {
      return 1;
    }
  }
  return 0;
}

std::optional<ReleaseInfo> parseLatestReleaseJson(const std::string& text, std::string* error)
{
  try {
    const nlohmann::json root = nlohmann::json::parse(text);
    if (!root.is_object()) {
      if (error) {
        *error = "GitHub release response was not a JSON object";
      }
      return std::nullopt;
    }

    if (root.value("draft", false) || root.value("prerelease", false)) {
      if (error) {
        *error = "GitHub's latest-release endpoint returned a draft or prerelease";
      }
      return std::nullopt;
    }

    ReleaseInfo info;
    info.tagName = root.value("tag_name", "");
    info.name = root.value("name", "");
    info.htmlUrl = root.value("html_url", "");
    info.publishedAt = root.value("published_at", "");
    info.body = root.value("body", "");

    if (info.tagName.empty()) {
      if (error) {
        *error = "GitHub release response did not include a tag name";
      }
      return std::nullopt;
    }
    if (!versionParts(info.tagName)) {
      if (error) {
        *error = "GitHub release response contained an invalid version tag: " + info.tagName;
      }
      return std::nullopt;
    }
    if (info.htmlUrl.empty()) {
      info.htmlUrl = k_downloadPageUrl;
    }
    return info;
  }
  catch (const std::exception& e) {
    if (error) {
      *error = e.what();
    }
    return std::nullopt;
  }
}

CheckResult parseGitHubReleaseHttpResponse(const std::string& responseText, const std::string& currentVersion)
{
  CheckResult result;
  int status = 0;
  bool inHeaders = false;
  std::string body;

  std::istringstream stream(responseText);
  std::string line;
  while (std::getline(stream, line)) {
    if (!line.empty() && line.back() == '\r') {
      line.pop_back();
    }

    if (line.starts_with("HTTP/")) {
      status = parseHttpStatus(line);
      inHeaders = true;
      body.clear();
      result.etag.clear();
      continue;
    }

    if (inHeaders) {
      if (line.empty()) {
        inHeaders = false;
        continue;
      }

      const std::size_t colon = line.find(':');
      if (colon != std::string::npos) {
        const std::string name = toLower(trim(line.substr(0, colon)));
        const std::string value = trim(line.substr(colon + 1));
        if (name == "etag") {
          result.etag = value;
        }
      }
      continue;
    }

    body += line;
    body += '\n';
  }

  if (status == 304) {
    result.status = CheckStatus::NotModified;
    return result;
  }

  if (status == 404) {
    result.status = CheckStatus::NoPublishedReleases;
    return result;
  }

  if (status < 200 || status >= 300) {
    result.status = CheckStatus::Failed;
    result.error = status == 0 ? "No HTTP response received from GitHub"
                               : "GitHub update check failed with HTTP status " + std::to_string(status);
    return result;
  }

  std::string parseError;
  const std::optional<ReleaseInfo> latest = parseLatestReleaseJson(body, &parseError);
  if (!latest) {
    result.status = CheckStatus::Failed;
    result.error = "Could not parse GitHub release response: " + parseError;
    return result;
  }

  return resultForRelease(currentVersion, *latest, std::move(result.etag));
}

CheckResult resolveCachedCheckResult(CheckResult response, const std::optional<CheckResult>& cachedResult)
{
  if (response.status != CheckStatus::NotModified) {
    return response;
  }

  if (
    !cachedResult ||
    (cachedResult->status != CheckStatus::UpdateAvailable && cachedResult->status != CheckStatus::UpToDate))
  {
    return failedResult("GitHub returned an unchanged release without a preceding successful update check");
  }

  CheckResult resolved = *cachedResult;
  if (!response.etag.empty()) {
    resolved.etag = std::move(response.etag);
  }
  return resolved;
}

CheckResult fetchLatestRelease(const CheckRequest& request)
{
  const CurlGlobalState& global = curlGlobalState();
  if (global.code != CURLE_OK) {
    return failedResult("Could not initialize libcurl: " + std::string{curl_easy_strerror(global.code)});
  }

  CurlEasyHandle curl{curl_easy_init()};
  if (!curl) {
    return failedResult("Could not create a libcurl easy handle");
  }

  CurlResponse response;
  char errorBuffer[CURL_ERROR_SIZE] = {};

  curl_slist* rawHeaders = nullptr;
  CurlHeaderList headers;
  if (
    !appendCurlHeader(rawHeaders, "Accept: application/vnd.github+json") ||
    !appendCurlHeader(rawHeaders, "X-GitHub-Api-Version: 2022-11-28") ||
    (!request.etag.empty() && !appendCurlHeader(rawHeaders, "If-None-Match: " + request.etag)))
  {
    headers.reset(rawHeaders);
    return failedResult("Could not allocate libcurl request headers");
  }
  headers.reset(rawHeaders);

  const std::string userAgent = "Entropy/" + request.currentVersion;
  curl_easy_setopt(curl.get(), CURLOPT_URL, k_latestReleaseApiUrl);
  curl_easy_setopt(curl.get(), CURLOPT_HTTPHEADER, headers.get());
  curl_easy_setopt(curl.get(), CURLOPT_FOLLOWLOCATION, 1L);
  curl_easy_setopt(curl.get(), CURLOPT_MAXREDIRS, 5L);
#if LIBCURL_VERSION_NUM >= 0x075500
  curl_easy_setopt(curl.get(), CURLOPT_PROTOCOLS_STR, "https");
  curl_easy_setopt(curl.get(), CURLOPT_REDIR_PROTOCOLS_STR, "https");
#else
  curl_easy_setopt(curl.get(), CURLOPT_PROTOCOLS, CURLPROTO_HTTPS);
  curl_easy_setopt(curl.get(), CURLOPT_REDIR_PROTOCOLS, CURLPROTO_HTTPS);
#endif
  curl_easy_setopt(curl.get(), CURLOPT_CONNECTTIMEOUT, 5L);
  curl_easy_setopt(curl.get(), CURLOPT_TIMEOUT, 15L);
  curl_easy_setopt(curl.get(), CURLOPT_NOSIGNAL, 1L);
  curl_easy_setopt(curl.get(), CURLOPT_USERAGENT, userAgent.c_str());
  curl_easy_setopt(curl.get(), CURLOPT_ERRORBUFFER, errorBuffer);
  curl_easy_setopt(curl.get(), CURLOPT_WRITEFUNCTION, writeCurlBody);
  curl_easy_setopt(curl.get(), CURLOPT_WRITEDATA, &response);
  curl_easy_setopt(curl.get(), CURLOPT_HEADERFUNCTION, writeCurlHeader);
  curl_easy_setopt(curl.get(), CURLOPT_HEADERDATA, &response);
#if defined(__linux__)
  configureLinuxCaStore(curl.get());
#endif

  const CURLcode curlCode = curl_easy_perform(curl.get());
  if (curlCode != CURLE_OK) {
    const std::string detail = errorBuffer[0] != '\0' ? errorBuffer : curl_easy_strerror(curlCode);
    return failedResult("GitHub update check failed through libcurl: " + detail);
  }

  long status = 0;
  curl_easy_getinfo(curl.get(), CURLINFO_RESPONSE_CODE, &status);

  CheckResult result;
  result.etag = response.etag;
  if (status == 304) {
    result.status = CheckStatus::NotModified;
    return result;
  }
  if (status == 404) {
    result.status = CheckStatus::NoPublishedReleases;
    return result;
  }
  if (status < 200 || status >= 300) {
    result.status = CheckStatus::Failed;
    result.error = status == 0 ? "No HTTP response received from GitHub"
                               : "GitHub update check failed with HTTP status " + std::to_string(status);
    return result;
  }

  std::string parseError;
  const std::optional<ReleaseInfo> latest = parseLatestReleaseJson(response.body, &parseError);
  if (!latest) {
    result.status = CheckStatus::Failed;
    result.error = "Could not parse GitHub release response: " + parseError;
    return result;
  }

  return resultForRelease(request.currentVersion, *latest, std::move(result.etag));
}

bool openUrlInDefaultBrowser(const std::string& url, std::string* error)
{
#ifdef _WIN32
  const HINSTANCE result = ShellExecuteA(nullptr, "open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
  if (reinterpret_cast<std::intptr_t>(result) <= 32) {
    if (error) {
      *error = "ShellExecute failed";
    }
    return false;
  }
  return true;
#elif defined(__APPLE__)
  registration::ShellProcessRunner runner;
  registration::CommandSpec command{.description = "Open URL", .executable = "open", .args = {url}};
  const registration::ProcessResult result = runner.run(command, {}, {});
  if (result.launchFailed || result.exitCode != 0) {
    if (error) {
      *error = result.failureMessage.empty() ? "Could not open URL" : result.failureMessage;
    }
    return false;
  }
  return true;
#else
  registration::ShellProcessRunner runner;
  registration::CommandSpec command{.description = "Open URL", .executable = "xdg-open", .args = {url}};
  const registration::ProcessResult result = runner.run(command, {}, {});
  if (result.launchFailed || result.exitCode != 0) {
    if (error) {
      *error = result.failureMessage.empty() ? "Could not open URL" : result.failureMessage;
    }
    return false;
  }
  return true;
#endif
}

void renderUpdateCheckWindow(
  CheckWindowState& state,
  bool automaticChecksEnabled,
  const std::function<void(bool)>& setAutomaticChecksEnabled)
{
  if (!state.open) {
    return;
  }

  const float windowWidth = updateCheckWindowWidth(state);
  const ImGuiViewport* viewport = ImGui::GetMainViewport();
  const ImVec2 center{
    viewport->WorkPos.x + 0.5f * viewport->WorkSize.x,
    viewport->WorkPos.y + 0.5f * viewport->WorkSize.y};

  ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2{0.5f, 0.5f});
  ImGui::SetNextWindowSizeConstraints(ImVec2{windowWidth, 0.0f}, ImVec2{windowWidth, FLT_MAX});
  ImGui::OpenPopup("Entropy Updates");

  bool open = state.open;
  if (!ImGui::BeginPopupModal("Entropy Updates", &open, ImGuiWindowFlags_AlwaysAutoResize)) {
    return;
  }
  state.open = open;

  if (state.checking) {
    renderWrappedLabel("Checking GitHub for Entropy updates...");
  }
  else if (!state.hasResult) {
    renderWrappedLabel("No update check has been run yet.");
  }
  else {
    switch (state.result.status) {
      case CheckStatus::UpdateAvailable:
        ImGui::TextUnformatted("An Entropy update is available.");
        ImGui::Spacing();
        ImGui::Text("Current version: %s", ENTROPY_VERSION);
        ImGui::Text("Latest version: %s", state.result.latestRelease.tagName.c_str());
        if (!state.result.latestRelease.name.empty()) {
          ImGui::TextWrapped("%s", state.result.latestRelease.name.c_str());
        }
        break;
      case CheckStatus::UpToDate:
      case CheckStatus::NotModified:
        ImGui::Text("Current version: %s", ENTROPY_VERSION);
        renderWrappedLabel("Entropy is up to date.");
        break;
      case CheckStatus::NoPublishedReleases:
        ImGui::Text("Current version: %s", ENTROPY_VERSION);
        renderWrappedLabel("No published Entropy releases were found on GitHub yet.");
        break;
      case CheckStatus::Failed:
        ImGui::TextUnformatted("Could not check for updates.");
        ImGui::Spacing();
        ImGui::TextWrapped("%s", state.result.error.c_str());
        break;
    }
  }

  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Spacing();

  bool automaticChecks = automaticChecksEnabled;
  if (ImGui::Checkbox("Check for updates automatically", &automaticChecks)) {
    setAutomaticChecksEnabled(automaticChecks);
  }

  ImGui::Spacing();

  if (ImGui::Button("Open Download Page")) {
    std::string error;
    if (!openUrlInDefaultBrowser(k_downloadPageUrl, &error)) {
      spdlog::warn("Failed to open Entropy download page: {}", error);
    }
  }
  ImGui::SameLine();
  if (ImGui::Button("Close")) {
    state.open = false;
    ImGui::CloseCurrentPopup();
  }

  ImGui::EndPopup();
}

} // namespace ui::updates
