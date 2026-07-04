#include "ui/updates/UpdateCheck.h"

#include <imgui/imgui.h>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cfloat>
#include <sstream>
#include <string_view>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <shellapi.h>
#endif

#ifndef ENTROPY_VERSION
#define ENTROPY_VERSION "0.0.0"
#endif

namespace entropy::ui::updates
{
namespace
{
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

std::vector<int> versionParts(std::string value)
{
  value = trim(std::move(value));
  if (!value.empty() && (value.front() == 'v' || value.front() == 'V')) {
    value.erase(value.begin());
  }

  std::vector<int> parts;
  std::size_t pos = 0;
  while (pos < value.size()) {
    while (pos < value.size() && !std::isdigit(static_cast<unsigned char>(value[pos]))) {
      if (value[pos] != '.') {
        return parts;
      }
      ++pos;
    }
    if (pos >= value.size()) {
      break;
    }

    std::size_t end = pos;
    while (end < value.size() && std::isdigit(static_cast<unsigned char>(value[end]))) {
      ++end;
    }

    int part = 0;
    const auto* beginPtr = value.data() + pos;
    const auto* endPtr = value.data() + end;
    static_cast<void>(std::from_chars(beginPtr, endPtr, part));
    parts.push_back(part);
    pos = end;
    if (pos < value.size() && value[pos] == '.') {
      ++pos;
    }
  }
  return parts;
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

std::string joinOutput(const std::vector<registration::ProcessOutputLine>& lines, registration::OutputStream stream)
{
  std::string text;
  for (const registration::ProcessOutputLine& line : lines) {
    if (line.stream != stream) {
      continue;
    }
    text += line.text;
    text += '\n';
  }
  return text;
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

int compareReleaseVersions(std::string lhs, std::string rhs)
{
  std::vector<int> lhsParts = versionParts(std::move(lhs));
  std::vector<int> rhsParts = versionParts(std::move(rhs));
  const std::size_t count = std::max(lhsParts.size(), rhsParts.size());
  lhsParts.resize(count, 0);
  rhsParts.resize(count, 0);

  for (std::size_t i = 0; i < count; ++i) {
    if (lhsParts[i] < rhsParts[i]) {
      return -1;
    }
    if (lhsParts[i] > rhsParts[i]) {
      return 1;
    }
  }
  return 0;
}

std::optional<ReleaseInfo> parseLatestReleaseJson(const std::string& text, std::string* error)
{
  try {
    const nlohmann::json root = nlohmann::json::parse(text);
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

  result.latestRelease = *latest;
  result.status =
    compareReleaseVersions(currentVersion, latest->tagName) < 0 ? CheckStatus::UpdateAvailable : CheckStatus::UpToDate;
  return result;
}

CheckResult fetchLatestRelease(registration::IProcessRunner& processRunner, const CheckRequest& request)
{
  registration::CommandSpec command;
  command.description = "Check Entropy updates";
  command.executable = "curl";
  command.args = {
    "-L",
    "--silent",
    "--show-error",
    "--connect-timeout",
    "5",
    "--max-time",
    "15",
    "-i",
    "-H",
    "Accept: application/vnd.github+json",
    "-H",
    "X-GitHub-Api-Version: 2022-11-28",
    "-H",
    "User-Agent: Entropy/" + request.currentVersion};
  if (!request.etag.empty()) {
    command.args.push_back("-H");
    command.args.push_back("If-None-Match: " + request.etag);
  }
  command.args.push_back(k_latestReleaseApiUrl);

  registration::ProcessOptions options;
  options.mergeStdErrIntoStdOut = false;
  const registration::ProcessResult processResult = processRunner.run(command, options, {});

  if (processResult.launchFailed) {
    return CheckResult{
      .status = CheckStatus::Failed,
      .error = processResult.failureMessage.empty() ? "Could not launch curl" : processResult.failureMessage};
  }
  if (processResult.exitCode != 0) {
    const std::string stderrText = joinOutput(processResult.outputLines, registration::OutputStream::Stderr);
    return CheckResult{
      .status = CheckStatus::Failed,
      .error =
        stderrText.empty() ? "curl failed with exit code " + std::to_string(processResult.exitCode) : trim(stderrText)};
  }

  return parseGitHubReleaseHttpResponse(
    joinOutput(processResult.outputLines, registration::OutputStream::Stdout),
    request.currentVersion);
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

void renderUpdateCheckWindow(CheckWindowState& state)
{
  if (!state.open) {
    return;
  }

  const float windowWidth = updateCheckWindowWidth(state);
  ImGui::SetNextWindowSizeConstraints(ImVec2{windowWidth, 0.0f}, ImVec2{windowWidth, FLT_MAX});
  if (!ImGui::Begin("Entropy Updates", &state.open, ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::End();
    return;
  }

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

  if (ImGui::Button("Open Download Page")) {
    std::string error;
    if (!openUrlInDefaultBrowser(k_downloadPageUrl, &error)) {
      spdlog::warn("Failed to open Entropy download page: {}", error);
    }
  }
  ImGui::SameLine();
  if (ImGui::Button("Close")) {
    state.open = false;
  }

  ImGui::End();
}

} // namespace entropy::ui::updates
