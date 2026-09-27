#include "ui/ImGuiWrapper.h"

#include "logic/app/Data.h"

#include <spdlog/fmt/std.h>
#include <spdlog/spdlog.h>

#include <chrono>
#include <functional>
#include <utility>

void ImGuiWrapper::requestUpdateCheck(bool manualCheck)
{
  using namespace ui::updates;

  if (m_updateCheckFuture.valid() && std::future_status::ready != m_updateCheckFuture.wait_for(std::chrono::seconds{0}))
  {
    if (manualCheck) {
      spdlog::info("User requested an update check while a GitHub update check was already in progress");
      m_updateCheckWindowState.open = true;
      m_updateCheckWindowState.manualCheck = true;
    }
    return;
  }

  if (manualCheck) {
    spdlog::info("User requested a GitHub update check for Entropy {}", ENTROPY_VERSION);
  }
  else {
    spdlog::info("Starting automatic GitHub update check for Entropy {}", ENTROPY_VERSION);
  }

  m_updateCheckWindowState.open = manualCheck;
  m_updateCheckWindowState.checking = true;
  m_updateCheckWindowState.manualCheck = manualCheck;
  m_updateCheckWindowState.hasResult = false;

  const std::string etag = m_updateCheckEtag;
  m_updateCheckFuture = std::async(std::launch::async, [etag]() {
    return fetchLatestRelease(CheckRequest{.currentVersion = ENTROPY_VERSION, .etag = etag});
  });

  if (m_postEmptyGlfwEvent) {
    m_postEmptyGlfwEvent();
  }
}

void ImGuiWrapper::requestAutomaticUpdateCheckIfNeeded()
{
  if (m_automaticUpdateCheckRequested || !m_appData.settings().automaticUpdateChecksEnabled()) {
    return;
  }

  m_automaticUpdateCheckRequested = true;
  requestUpdateCheck(false);
}

void ImGuiWrapper::processUpdateCheckFuture()
{
  using namespace std::chrono_literals;

  if (!m_updateCheckFuture.valid() || std::future_status::ready != m_updateCheckFuture.wait_for(0ms)) {
    return;
  }

  ui::updates::CheckResult result;
  try {
    result = m_updateCheckFuture.get();
  }
  catch (const std::exception& e) {
    result.status = ui::updates::CheckStatus::Failed;
    result.error = e.what();
  }

  result = ui::updates::resolveCachedCheckResult(std::move(result), m_cachedUpdateCheckResult);
  const bool manualCheck = m_updateCheckWindowState.manualCheck;
  if (result.status == ui::updates::CheckStatus::UpdateAvailable || result.status == ui::updates::CheckStatus::UpToDate)
  {
    m_cachedUpdateCheckResult = result;
    if (!result.etag.empty()) {
      m_updateCheckEtag = result.etag;
    }
  }

  const bool showAutomaticResult = !manualCheck && result.status == ui::updates::CheckStatus::UpdateAvailable;

  switch (result.status) {
    case ui::updates::CheckStatus::UpdateAvailable:
      spdlog::info(
        "Entropy update available: installed version {}, latest release {} ({})",
        ENTROPY_VERSION,
        result.latestRelease.tagName,
        result.latestRelease.htmlUrl);
      break;
    case ui::updates::CheckStatus::UpToDate:
    case ui::updates::CheckStatus::NotModified:
      spdlog::info("Entropy {} is up to date", ENTROPY_VERSION);
      break;
    case ui::updates::CheckStatus::NoPublishedReleases:
      spdlog::warn("GitHub reported no published Entropy releases; update availability could not be determined");
      break;
    case ui::updates::CheckStatus::Failed:
      spdlog::warn("GitHub update check failed: {}. Entropy will continue without update information", result.error);
      break;
  }

  m_updateCheckWindowState.result = std::move(result);
  m_updateCheckWindowState.checking = false;
  m_updateCheckWindowState.hasResult = true;
  m_updateCheckWindowState.open = m_updateCheckWindowState.manualCheck || showAutomaticResult;

  if (m_postEmptyGlfwEvent) {
    m_postEmptyGlfwEvent();
  }
}
