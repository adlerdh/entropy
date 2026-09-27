#include "ui/ImGuiWrapper.h"

#include "logic/app/Data.h"
#include "ui/imgui/ImageSelection.h"
#include "ui/imgui/Layout.h"
#include "ui/imgui/Workspace.h"
#include "ui/menus/MainMenuBar.h"

#ifdef __APPLE__
#include "ui/menus/MacNativeMainMenu.h"
#elif defined(_WIN32)
#include "ui/menus/WinNativeMainMenu.h"
#endif

#include <spdlog/fmt/std.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <functional>
#include <iterator>
#include <unordered_map>
#include <utility>

namespace fs = std::filesystem;

using namespace ui::imgui_detail;

std::filesystem::path ImGuiWrapper::defaultProjectSaveDirectory()
{
  if (m_appData.projectFileName()) {
    return m_appData.projectFileName()->parent_path();
  }
  const auto refImageUid = m_appData.refImageUid();
  const Image* refImage = refImageUid ? m_appData.image(*refImageUid) : nullptr;
  return refImage ? refImage->header().fileName().parent_path() : fs::path{};
}

std::string ImGuiWrapper::defaultProjectSaveName()
{
  if (m_appData.projectFileName()) {
    return m_appData.projectFileName()->filename().string();
  }
  const auto refImageUid = m_appData.refImageUid();
  const Image* refImage = refImageUid ? m_appData.image(*refImageUid) : nullptr;
  return refImage ? (refImage->header().fileName().stem().string() + ".json") : std::string{"project.json"};
}

std::string ImGuiWrapper::defaultLayoutsSaveName()
{
  if (m_appData.projectFileName()) {
    return m_appData.projectFileName()->stem().string() + "-layouts.json";
  }
  const auto refImageUid = m_appData.refImageUid();
  const Image* refImage = refImageUid ? m_appData.image(*refImageUid) : nullptr;
  return refImage ? (refImage->header().fileName().stem().string() + "-layouts.json") : std::string{"layouts.json"};
}

std::vector<std::string> ImGuiWrapper::layoutNames()
{
  const auto& layouts = m_appData.windowData().layouts();
  std::vector<std::string> baseNames;
  baseNames.reserve(layouts.size());

  std::unordered_map<std::string, std::size_t> baseNameCounts;
  for (std::size_t index = 0; index < layouts.size(); ++index) {
    baseNames.emplace_back(m_appData.windowData().layoutDisplayName(index));
    ++baseNameCounts[baseNames.back()];
  }

  std::vector<std::string> names;
  names.reserve(layouts.size());
  std::unordered_map<std::string, std::size_t> seenBaseNameCounts;
  for (std::size_t index = 0; index < layouts.size(); ++index) {
    std::string displayName = baseNames.at(index);
    const bool managedLightbox = LayoutKind::Lightbox == layouts.at(index).kind();
    const std::string imageName = managedLightbox ? layoutImageDisplayName(m_appData, layouts.at(index)) : "";
    if (managedLightbox && !imageName.empty()) {
      displayName += " - " + imageName;
    }
    else if (baseNameCounts.at(displayName) > 1) {
      displayName += " " + std::to_string(++seenBaseNameCounts[displayName]);
    }
    names.emplace_back(std::to_string(index + 1) + ". " + displayName);
  }
  return names;
}

std::filesystem::path ImGuiWrapper::defaultLayoutsSaveDirectory()
{
  return defaultProjectSaveDirectory();
}

void ImGuiWrapper::renderMenus(ProjectLoadState projectLoadState, bool backgroundTaskRunning)
{
  const auto getActiveImageIndex = std::bind_front(&ImGuiWrapper::getActiveImageIndex, this);
  const auto setActiveImageIndex = std::bind_front(&ImGuiWrapper::setActiveImageIndex, this);
  const auto performMenuAction = std::bind_front(&ImGuiWrapper::performMenuAction, this);
  const auto isMenuActionEnabled = std::bind_front(&ImGuiWrapper::isMenuActionEnabled, this);
  const auto isMenuActionChecked = std::bind_front(&ImGuiWrapper::isMenuActionChecked, this);
  const auto defaultProjectSaveDirectory = std::bind_front(&ImGuiWrapper::defaultProjectSaveDirectory, this);
  const auto defaultProjectSaveName = std::bind_front(&ImGuiWrapper::defaultProjectSaveName, this);
  const auto defaultLayoutsSaveName = std::bind_front(&ImGuiWrapper::defaultLayoutsSaveName, this);
  const auto layoutNames = std::bind_front(&ImGuiWrapper::layoutNames, this);
  const auto saveUserSettingsToDefault = std::bind_front(&ImGuiWrapper::saveUserSettingsToDefault, this);
  const auto defaultLayoutsSaveDirectory = std::bind_front(&ImGuiWrapper::defaultLayoutsSaveDirectory, this);
  const auto activeImageUidForMenu = m_appData.activeImageUid();
  const auto refImageUidForMenu = m_appData.refImageUid();
  const bool activeImageCanSelfWarp =
    activeImageUidForMenu && imageIsOnlyNonWarpImage(m_appData, *activeImageUidForMenu);
  const bool canLoadDeformationFieldForActiveImage =
    ProjectLoadState::Loaded == projectLoadState && !backgroundTaskRunning && activeImageUidForMenu &&
    (!refImageUidForMenu || *activeImageUidForMenu != *refImageUidForMenu || activeImageCanSelfWarp) &&
    m_loadAndAssignDeformationField;

  // Native menus retain this callback after render() returns, so every frame-local callable it uses must be owned.
  const auto clearRecents = [this, saveUserSettingsToDefault]() {
    const std::size_t projectCount = m_appData.settings().recentProjectFiles().size();
    const std::size_t imageGroupCount = m_appData.settings().recentImageGroups().size();
    const std::size_t dicomGroupCount = m_appData.settings().recentDicomGroups().size();
    m_appData.settings().setRecentProjectFiles({});
    m_appData.settings().setRecentImageGroups({});
    m_appData.settings().setRecentDicomGroups({});
    saveUserSettingsToDefault();
    spdlog::info(
      "Cleared recent data history: {} project(s), {} image group(s), and {} DICOM group(s)",
      projectCount,
      imageGroupCount,
      dicomGroupCount);
  };

  const MainMenuBarCallbacks mainMenuCallbacks{
    .openImageFiles = m_openImageFiles,
    .addImageFiles = m_addImageFiles,
    .openDicomFolders = m_openDicomFolders,
    .requestDicomFolderPathDialog = [this]() { m_appData.guiData().m_showDicomFolderPathPopup = true; },
    .recentProjectFiles = [this]() { return m_appData.settings().recentProjectFiles(); },
    .recentImageGroups =
      [this]() {
        std::vector<std::vector<fs::path>> groups;
        const auto& recentGroups = m_appData.settings().recentImageGroups();
        groups.reserve(recentGroups.size());
        std::transform(
          recentGroups.begin(),
          recentGroups.end(),
          std::back_inserter(groups),
          [](const RecentPathGroup& group) { return group.paths; });
        return groups;
      },
    .recentDicomGroups =
      [this]() {
        std::vector<std::vector<fs::path>> groups;
        const auto& recentGroups = m_appData.settings().recentDicomGroups();
        groups.reserve(recentGroups.size());
        std::transform(
          recentGroups.begin(),
          recentGroups.end(),
          std::back_inserter(groups),
          [](const RecentPathGroup& group) { return group.paths; });
        return groups;
      },
    .clearRecents = clearRecents,
    .addSegmentationFile = m_addSegmentationFile,
    .loadInverseWarpForActiveImage =
      [this](const fs::path& fileName) {
        const auto imageUid = m_appData.activeImageUid();
        const auto refImageUid = m_appData.refImageUid();
        if (
          !imageUid || (refImageUid && *imageUid == *refImageUid && !imageIsOnlyNonWarpImage(m_appData, *imageUid)) ||
          !m_loadAndAssignDeformationField)
        {
          return;
        }

        const std::optional<uuids::uuid> referenceUid =
          refImageUid ? refImageUid : std::optional<uuids::uuid>{imageUid};
        m_loadAndAssignDeformationField(*imageUid, fileName, false, referenceUid);
      },
    .loadForwardWarpForActiveImage =
      [this](const fs::path& fileName) {
        const auto imageUid = m_appData.activeImageUid();
        const auto refImageUid = m_appData.refImageUid();
        if (
          !imageUid || (refImageUid && *imageUid == *refImageUid && !imageIsOnlyNonWarpImage(m_appData, *imageUid)) ||
          !m_loadAndAssignDeformationField)
        {
          return;
        }

        m_loadAndAssignDeformationField(*imageUid, fileName, true, std::nullopt);
      },
    .openProjectFile = m_openProjectFile,
    .saveProject = m_saveProject,
    .saveProjectAs = m_saveProjectAs,
    .projectFileName = [this]() { return m_appData.projectFileName(); },
    .defaultProjectSaveDirectory = defaultProjectSaveDirectory,
    .defaultProjectSaveName = defaultProjectSaveName,
    .closeProject = m_closeProject,
    .quitApp = m_requestQuitApp,
    .loadLayoutsFile = m_loadLayoutsFile,
    .saveLayoutsFile = m_saveLayoutsFile,
    .resetProjectSettings = m_resetProjectSettings,
    .defaultLayoutsSaveDirectory = defaultLayoutsSaveDirectory,
    .defaultLayoutsSaveName = defaultLayoutsSaveName,
    .layoutNames = layoutNames,
    .currentLayoutIndex = [this]() { return m_appData.windowData().currentLayoutIndex(); },
    .setCurrentLayoutIndex =
      [this](std::size_t index) {
        m_appData.windowData().setCurrentLayoutIndex(index);
        if (m_appData.renderSettings().m_synchronizeThreeDCameras) {
          m_appData.windowData().synchronizeCurrentLayoutThreeDCameras();
        }
        if (m_postEmptyGlfwEvent) {
          m_postEmptyGlfwEvent();
        }
      },
    .cycleLayouts =
      [this](int step) {
        m_appData.windowData().cycleCurrentLayout(step);
        if (m_appData.renderSettings().m_synchronizeThreeDCameras) {
          m_appData.windowData().synchronizeCurrentLayoutThreeDCameras();
        }
        if (m_postEmptyGlfwEvent) {
          m_postEmptyGlfwEvent();
        }
      },
    .imageNames =
      [this]() {
        std::vector<std::string> names;
        names.reserve(m_appData.numImages());
        for (std::size_t i = 0; i < m_appData.numImages(); ++i) {
          names.emplace_back(std::to_string(i + 1) + ". " + getImageDisplayAndFileNames(i).first);
        }
        return names;
      },
    .activeImageIndex = getActiveImageIndex,
    .setActiveImageIndex = setActiveImageIndex,
    .performAction = performMenuAction,
    .isActionEnabled = isMenuActionEnabled,
    .isActionChecked = isMenuActionChecked,
    .showAbout = [this]() { m_appData.guiData().m_showAboutDialog = true; },
    .showKeyboardShortcuts = [this]() { m_appData.guiData().m_showKeyboardShortcutsWindow = true; },
    .checkForUpdates = [this]() { requestUpdateCheck(true); },
    .openDownloadPage =
      []() {
        std::string error;
        if (!ui::updates::openUrlInDefaultBrowser(ui::updates::k_downloadPageUrl, &error)) {
          spdlog::warn("Failed to open Entropy download page: {}", error);
        }
      },
    .canOpenProject = ProjectLoadState::Loading != projectLoadState && !backgroundTaskRunning,
    .canAddImage = ProjectLoadState::Loaded == projectLoadState && !backgroundTaskRunning,
    .canAddSegmentation =
      ProjectLoadState::Loaded == projectLoadState && !backgroundTaskRunning && m_appData.activeImageUid(),
    .canLoadDeformationFieldForActiveImage = canLoadDeformationFieldForActiveImage,
    .canSaveProject = ProjectLoadState::Loaded == projectLoadState && !backgroundTaskRunning,
    .canCloseProject = ProjectLoadState::Empty != projectLoadState && !backgroundTaskRunning,
    .canUseLayouts = ProjectLoadState::Loaded == projectLoadState && !backgroundTaskRunning};

#ifdef __APPLE__
  updateMacOSNativeMainMenu(mainMenuCallbacks);
#elif defined(_WIN32)
  updateWindowsNativeMainMenu(m_window, mainMenuCallbacks);
#else
  renderMainMenuBar(m_appData.guiData(), mainMenuCallbacks);
#endif

  renderEmptyWorkspace(
    projectLoadState,
    m_appData.settings(),
    m_openImageFiles,
    m_openDicomFolders,
    [this]() { m_appData.guiData().m_showDicomFolderPathPopup = true; },
    m_openProjectFile,
    clearRecents);
}
