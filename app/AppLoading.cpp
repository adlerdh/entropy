#include "EntropyApp.h"

#include "logic/app/LoadingStatusItems.h"
#include "logic/serialization/ProjectSerialization.h"
#include "ui/dialogs/InputLoadErrorDialog.h"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <filesystem>
#include <future>
#include <mutex>
#include <optional>
#include <utility>

namespace fs = std::filesystem;

void EntropyApp::reportInputLoadFailure(std::string inputType, std::optional<fs::path> path, std::string cause)
{
  {
    std::scoped_lock lock(m_pendingInputLoadFailuresMutex);
    m_pendingInputLoadFailures.push_back(
      {.inputType = std::move(inputType), .path = std::move(path), .cause = std::move(cause)});
  }
  GlfwWrapper::postEmptyEvent();
}

void EntropyApp::showNextInputLoadFailure()
{
  std::optional<PendingInputLoadFailure> failure;
  bool moreFailuresPending = false;
  {
    std::scoped_lock lock(m_pendingInputLoadFailuresMutex);
    if (!m_pendingInputLoadFailures.empty()) {
      failure = std::move(m_pendingInputLoadFailures.front());
      m_pendingInputLoadFailures.pop_front();
      moreFailuresPending = !m_pendingInputLoadFailures.empty();
    }
  }
  if (failure) {
    native_dialog::showInputLoadErrorDialog(
      {.inputType = std::move(failure->inputType),
       .path = std::move(failure->path),
       .cause = std::move(failure->cause)});
  }
  if (moreFailuresPending) {
    GlfwWrapper::postEmptyEvent();
  }
}

void EntropyApp::loadImagesFromParams(const InputParams& params)
{
  if (!params.dicomPaths.empty()) {
    m_pendingLayoutsFile = params.layoutsFile;
    performOpenDicomSeriesFolders(params.dicomPaths);
    return;
  }

  const std::optional<fs::path> projectFileName = params.imageFiles.empty() ? params.projectFile : std::nullopt;
  m_pendingLayoutsFile = params.layoutsFile;
  m_data.setProject(serialize::createProjectFromInputParams(params));
  m_data.setProjectFileName(projectFileName);

  startAsyncImageLoad(
    "Loading project...",
    [this]() { return loadProject(m_data.project()); },
    [this]() {
      m_data.clearProjectData();
      m_data.state().setProjectLoadState(ProjectLoadState::Failed);
      m_data.state().setAnimating(false);
      m_glfw.setEventProcessingMode(EventProcessingMode::Wait);
    },
    true,
    loading_status::projectItems(m_data.project()));
}

void EntropyApp::beginLoadingStatus(std::string title, std::vector<GuiData::LoadingStatusItem> items)
{
  if (!m_data.guiData().m_loadingStatus) {
    m_data.guiData().m_loadingStatus = std::make_shared<GuiData::LoadingStatus>();
  }

  std::scoped_lock lock(m_data.guiData().m_loadingStatus->mutex);
  m_data.guiData().m_loadingStatus->title = std::move(title);
  m_data.guiData().m_loadingStatus->items = std::move(items);
  m_data.guiData().m_loadingStatus->visible = !m_data.guiData().m_loadingStatus->items.empty();
}

void EntropyApp::markLoadingStatusItemLoaded(GuiData::LoadingStatusItem::Kind kind, const fs::path& fileName)
{
  if (!m_data.guiData().m_loadingStatus) {
    return;
  }

  std::scoped_lock lock(m_data.guiData().m_loadingStatus->mutex);
  auto& items = m_data.guiData().m_loadingStatus->items;
  const auto item = std::find_if(items.begin(), items.end(), [&](const GuiData::LoadingStatusItem& candidate) {
    return !candidate.loaded && candidate.kind == kind && loading_status::equivalentPath(candidate.fileName, fileName);
  });
  if (item != items.end()) {
    item->loaded = true;
  }
}

void EntropyApp::hideLoadingStatus()
{
  if (!m_data.guiData().m_loadingStatus) {
    return;
  }

  std::scoped_lock lock(m_data.guiData().m_loadingStatus->mutex);
  m_data.guiData().m_loadingStatus->visible = false;
  m_data.guiData().m_loadingStatus->items.clear();
}

void EntropyApp::startAsyncImageLoad(
  const std::string& windowTitleStatusArg,
  std::function<bool()> loadTask,
  std::function<void()> onLoadFailed,
  bool showLoadingOverlay,
  std::vector<GuiData::LoadingStatusItem> loadingItems,
  std::string loadingStatusTitle)
{
  if (m_futureLoadProject.valid()) {
    m_futureLoadProject.wait();
    m_futureLoadProject = {};
  }
  if (m_futureDiscoverDicom.valid()) {
    m_futureDiscoverDicom.wait();
    m_futureDiscoverDicom = {};
  }

  m_imageLoadCancelled = false;
  m_imagesReady = false;
  m_imageLoadFailed = false;
  m_data.guiData().m_visibleImageCountDuringLoad = m_data.numImages();
  beginLoadingStatus(std::move(loadingStatusTitle), std::move(loadingItems));

  m_glfw.setWindowTitleStatus(windowTitleStatusArg);
  m_glfw.setEventProcessingMode(EventProcessingMode::Poll);
  if (showLoadingOverlay) {
    m_data.state().setProjectLoadState(ProjectLoadState::Loading);
  }
  m_data.state().setAnimating(true);
  GlfwWrapper::postEmptyEvent();

  auto onProjectLoadingDone = [this, onLoadFailed = std::move(onLoadFailed)](bool projectLoadedSuccessfully) {
    if (projectLoadedSuccessfully) {
      m_imagesReady = true;
      m_imageLoadFailed = false;
      GlfwWrapper::postEmptyEvent();
      spdlog::debug("Done loading images");
    }
    else {
      if (m_imageLoadCancelled) {
        spdlog::info("Image loading was cancelled; the requested load will not be completed");
      }
      else {
        spdlog::error("Image loading failed; the requested load will not be completed");
      }
      if (onLoadFailed) {
        onLoadFailed();
      }
      hideLoadingStatus();
      m_data.guiData().m_visibleImageCountDuringLoad = std::nullopt;
      m_imagesReady = false;
      m_imageLoadFailed = false;
      GlfwWrapper::postEmptyEvent();
    }
  };

  m_futureLoadProject = std::async(
    std::launch::async,
    [this, loadTask = std::move(loadTask), onProjectLoadingDone = std::move(onProjectLoadingDone)]() mutable {
      bool loaded = false;
      try {
        if (loadTask) {
          loaded = loadTask();
        }
      }
      catch (const std::exception& e) {
        spdlog::error("Exception while loading images: {}", e.what());
        reportInputLoadFailure("input data", std::nullopt, e.what());
      }
      catch (...) {
        spdlog::error("Unknown exception while loading images");
        reportInputLoadFailure("input data", std::nullopt, "An unknown error occurred while loading the input.");
      }

      onProjectLoadingDone(loaded);
    });
}
