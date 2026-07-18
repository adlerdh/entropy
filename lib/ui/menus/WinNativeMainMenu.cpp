#include "ui/menus/WinNativeMainMenu.h"

#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>
#include <windows.h>
#include <commctrl.h>

#include <filesystem>
#include <memory>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace
{
constexpr UINT k_openImageCommand = 1001;
constexpr UINT k_openProjectCommand = 1002;
constexpr UINT k_addImageCommand = 1003;
constexpr UINT k_openDicomSeriesCommand = 1008;
constexpr UINT k_addDicomSeriesCommand = 1009;
constexpr UINT k_loadInverseWarpCommand = 1010;
constexpr UINT k_loadForwardWarpCommand = 1011;
constexpr UINT k_addSegmentationCommand = 1004;
constexpr UINT k_saveProjectCommand = 1005;
constexpr UINT k_saveProjectAsCommand = 1006;
constexpr UINT k_closeProjectCommand = 1007;
constexpr UINT k_loadLayoutsCommand = 1101;
constexpr UINT k_saveLayoutsCommand = 1102;
constexpr UINT k_previousLayoutCommand = 1103;
constexpr UINT k_nextLayoutCommand = 1104;
constexpr UINT k_showAboutCommand = 1105;
constexpr UINT k_quitCommand = 1106;
constexpr UINT k_showKeyboardShortcutsCommand = 1107;
constexpr UINT k_checkForUpdatesCommand = 1108;
constexpr UINT k_openDownloadPageCommand = 1109;
constexpr UINT k_selectLayoutCommandBase = 1200;
constexpr UINT k_selectActiveImageCommandBase = 1400;
constexpr UINT k_actionCommandBase = 2000;
constexpr UINT k_actionCommandEnd = 2300;
constexpr UINT k_recentProjectCommandBase = 50000;
constexpr UINT k_recentImageCommandBase = 50100;
constexpr UINT k_recentImageGroupCommandBase = 50200;
constexpr UINT k_recentDicomCommandBase = 50300;
constexpr UINT k_clearRecentsCommand = 50400;
constexpr UINT k_recentCommandCount = 100;

constexpr UINT_PTR k_menuSubclassId = 1;

enum class RecentCommandKind
{
  Project,
  ImageGroup,
  DicomGroup
};

struct RecentCommand
{
  RecentCommandKind kind = RecentCommandKind::Project;
  std::vector<std::filesystem::path> paths;
};

struct MenuState
{
  HMENU mainMenu = nullptr;
  HMENU layoutsMenu = nullptr;
  HMENU activeImagesMenu = nullptr;
  HMENU openRecentMenu = nullptr;
  std::unordered_map<UINT, RecentCommand> recentCommands;
  std::string recentMenuSignature;
  MainMenuBarCallbacks callbacks;
};

std::unordered_map<HWND, std::unique_ptr<MenuState>> g_menuStates;

HWND hwndFromGlfw(GLFWwindow* window)
{
  return window ? glfwGetWin32Window(window) : nullptr;
}

bool isMenuCommand(UINT command)
{
  if (command >= k_actionCommandBase && command < k_actionCommandEnd) {
    return true;
  }

  if (command >= k_selectLayoutCommandBase && command < k_selectActiveImageCommandBase) {
    return true;
  }

  if (command >= k_selectActiveImageCommandBase && command < k_actionCommandBase) {
    return true;
  }

  if (
    (command >= k_recentProjectCommandBase && command < k_recentProjectCommandBase + k_recentCommandCount) ||
    (command >= k_recentImageCommandBase && command < k_recentImageCommandBase + k_recentCommandCount) ||
    (command >= k_recentImageGroupCommandBase && command < k_recentImageGroupCommandBase + k_recentCommandCount) ||
    (command >= k_recentDicomCommandBase && command < k_recentDicomCommandBase + k_recentCommandCount))
  {
    return true;
  }

  switch (command) {
    case k_openImageCommand:
    case k_openProjectCommand:
    case k_addImageCommand:
    case k_openDicomSeriesCommand:
    case k_addDicomSeriesCommand:
    case k_loadInverseWarpCommand:
    case k_loadForwardWarpCommand:
    case k_addSegmentationCommand:
    case k_saveProjectCommand:
    case k_saveProjectAsCommand:
    case k_closeProjectCommand:
    case k_loadLayoutsCommand:
    case k_saveLayoutsCommand:
    case k_previousLayoutCommand:
    case k_nextLayoutCommand:
    case k_showAboutCommand:
    case k_showKeyboardShortcutsCommand:
    case k_checkForUpdatesCommand:
    case k_openDownloadPageCommand:
    case k_clearRecentsCommand:
    case k_quitCommand:
      return true;
    default:
      return false;
  }
}

std::wstring widenUtf8(const std::string& value)
{
  if (value.empty()) {
    return {};
  }

  const int size = MultiByteToWideChar(CP_UTF8, 0, value.c_str(), -1, nullptr, 0);
  if (size <= 0) {
    return {};
  }

  std::wstring result(static_cast<std::size_t>(size - 1), L'\0');
  MultiByteToWideChar(CP_UTF8, 0, value.c_str(), -1, result.data(), size);
  return result;
}

bool recentPathsExist(const std::vector<std::filesystem::path>& paths)
{
  if (paths.empty()) {
    return false;
  }

  for (const auto& path : paths) {
    std::error_code error;
    if (!std::filesystem::exists(path, error)) {
      return false;
    }
  }
  return true;
}

std::wstring recentPathLabel(const std::vector<std::filesystem::path>& paths)
{
  if (paths.empty()) {
    return L"Untitled";
  }

  const auto fileName = paths.front().filename();
  std::wstring label = fileName.empty() ? paths.front().wstring() : fileName.wstring();
  if (paths.size() > 1) {
    label += L" (+" + std::to_wstring(paths.size() - 1) + L" more)";
  }
  return label;
}

std::string buildRecentMenuSignature(const MainMenuBarCallbacks& callbacks)
{
  std::ostringstream stream;
  const auto projects =
    callbacks.recentProjectFiles ? callbacks.recentProjectFiles() : std::vector<std::filesystem::path>{};
  const auto imageGroups =
    callbacks.recentImageGroups ? callbacks.recentImageGroups() : std::vector<std::vector<std::filesystem::path>>{};
  const auto dicomGroups =
    callbacks.recentDicomGroups ? callbacks.recentDicomGroups() : std::vector<std::vector<std::filesystem::path>>{};

  stream << "projects:";
  for (const auto& project : projects) {
    stream << project.string() << '\n';
  }
  stream << "images:";
  for (const auto& group : imageGroups) {
    stream << '[';
    for (const auto& path : group) {
      stream << path.string() << '\n';
    }
    stream << ']';
  }
  stream << "dicom:";
  for (const auto& group : dicomGroups) {
    stream << '[';
    for (const auto& path : group) {
      stream << path.string() << '\n';
    }
    stream << ']';
  }
  return stream.str();
}

void enableMenuCommand(const MenuState& state, UINT command, bool enabled)
{
  if (!state.mainMenu) {
    return;
  }

  EnableMenuItem(state.mainMenu, command, MF_BYCOMMAND | (enabled ? MF_ENABLED : MF_GRAYED));
}

void updateEnabledState(const MenuState& state)
{
  const auto& callbacks = state.callbacks;

  enableMenuCommand(state, k_openImageCommand, callbacks.canOpenProject && callbacks.openImageFiles);
  enableMenuCommand(state, k_openProjectCommand, callbacks.canOpenProject && callbacks.openProjectFile);
  enableMenuCommand(state, k_openDicomSeriesCommand, callbacks.canOpenProject && callbacks.openDicomFolders);
  enableMenuCommand(state, k_addImageCommand, callbacks.canAddImage && callbacks.addImageFiles);
  enableMenuCommand(state, k_addDicomSeriesCommand, callbacks.canAddImage && callbacks.openDicomFolders);
  enableMenuCommand(
    state,
    k_loadInverseWarpCommand,
    callbacks.canLoadDeformationFieldForActiveImage && callbacks.loadInverseWarpForActiveImage);
  enableMenuCommand(
    state,
    k_loadForwardWarpCommand,
    callbacks.canLoadDeformationFieldForActiveImage && callbacks.loadForwardWarpForActiveImage);
  enableMenuCommand(state, k_addSegmentationCommand, callbacks.canAddSegmentation && callbacks.addSegmentationFile);
  enableMenuCommand(state, k_saveProjectCommand, callbacks.canSaveProject && callbacks.saveProject);
  enableMenuCommand(state, k_saveProjectAsCommand, callbacks.canSaveProject && callbacks.saveProjectAs);
  enableMenuCommand(state, k_closeProjectCommand, callbacks.canCloseProject && callbacks.closeProject);
  enableMenuCommand(state, k_loadLayoutsCommand, callbacks.canUseLayouts && callbacks.loadLayoutsFile);
  enableMenuCommand(state, k_saveLayoutsCommand, callbacks.canUseLayouts && callbacks.saveLayoutsFile);
  enableMenuCommand(state, k_previousLayoutCommand, callbacks.canUseLayouts && callbacks.cycleLayouts);
  enableMenuCommand(state, k_nextLayoutCommand, callbacks.canUseLayouts && callbacks.cycleLayouts);
  enableMenuCommand(state, k_quitCommand, callbacks.quitApp != nullptr);

  const auto layoutNames = callbacks.layoutNames ? callbacks.layoutNames() : std::vector<std::string>{};
  for (std::size_t i = 0; i < layoutNames.size(); ++i) {
    enableMenuCommand(
      state,
      k_selectLayoutCommandBase + static_cast<UINT>(i),
      callbacks.canUseLayouts && callbacks.setCurrentLayoutIndex);
  }

  const auto imageNames = callbacks.imageNames ? callbacks.imageNames() : std::vector<std::string>{};
  const std::size_t activeImageIndex = callbacks.activeImageIndex ? callbacks.activeImageIndex() : 0;
  for (std::size_t i = 0; i < imageNames.size(); ++i) {
    const UINT command = k_selectActiveImageCommandBase + static_cast<UINT>(i);
    enableMenuCommand(state, command, callbacks.canAddImage && callbacks.setActiveImageIndex);
    CheckMenuItem(state.mainMenu, command, MF_BYCOMMAND | (i == activeImageIndex ? MF_CHECKED : MF_UNCHECKED));
  }

  for (UINT command = k_actionCommandBase; command < k_actionCommandEnd; ++command) {
    const auto action = static_cast<MainMenuAction>(command - k_actionCommandBase);
    enableMenuCommand(state, command, main_menu::actionEnabled(callbacks, action));
    CheckMenuItem(
      state.mainMenu,
      command,
      MF_BYCOMMAND | (main_menu::actionChecked(callbacks, action) ? MF_CHECKED : MF_UNCHECKED));
  }

  for (const auto& [command, recentCommand] : state.recentCommands) {
    enableMenuCommand(state, command, callbacks.canOpenProject && recentPathsExist(recentCommand.paths));
  }
  enableMenuCommand(state, k_clearRecentsCommand, callbacks.clearRecents != nullptr);
}

void handleMenuCommand(const MenuState& state, UINT command)
{
  const auto& callbacks = state.callbacks;

  if (command >= k_actionCommandBase && command < k_actionCommandEnd) {
    main_menu::performAction(callbacks, static_cast<MainMenuAction>(command - k_actionCommandBase));
    return;
  }

  if (command >= k_selectActiveImageCommandBase && command < k_actionCommandBase) {
    if (callbacks.setActiveImageIndex) {
      callbacks.setActiveImageIndex(command - k_selectActiveImageCommandBase);
    }
    return;
  }

  if (command >= k_selectLayoutCommandBase && command < k_selectActiveImageCommandBase) {
    if (callbacks.setCurrentLayoutIndex) {
      callbacks.setCurrentLayoutIndex(command - k_selectLayoutCommandBase);
    }
    return;
  }

  const auto recentCommand = state.recentCommands.find(command);
  if (recentCommand != std::end(state.recentCommands)) {
    const RecentCommand& recent = recentCommand->second;
    switch (recent.kind) {
      case RecentCommandKind::Project:
        if (callbacks.canOpenProject && callbacks.openProjectFile && !recent.paths.empty()) {
          callbacks.openProjectFile(recent.paths.front());
        }
        break;
      case RecentCommandKind::ImageGroup:
        if (callbacks.canOpenProject && callbacks.openImageFiles) {
          callbacks.openImageFiles(recent.paths);
        }
        break;
      case RecentCommandKind::DicomGroup:
        if (callbacks.canOpenProject && callbacks.openDicomFolders) {
          callbacks.openDicomFolders(recent.paths);
        }
        break;
    }
    return;
  }

  switch (command) {
    case k_openImageCommand:
      main_menu::openImage(callbacks);
      break;
    case k_openProjectCommand:
      main_menu::openProject(callbacks);
      break;
    case k_openDicomSeriesCommand:
      main_menu::openDicomSeries(callbacks);
      break;
    case k_addDicomSeriesCommand:
      main_menu::addDicomSeries(callbacks);
      break;
    case k_addImageCommand:
      main_menu::addImage(callbacks);
      break;
    case k_loadInverseWarpCommand:
      main_menu::loadInverseWarpForActiveImage(callbacks);
      break;
    case k_loadForwardWarpCommand:
      main_menu::loadForwardWarpForActiveImage(callbacks);
      break;
    case k_addSegmentationCommand:
      main_menu::addSegmentation(callbacks);
      break;
    case k_saveProjectCommand:
      main_menu::saveProject(callbacks);
      break;
    case k_saveProjectAsCommand:
      main_menu::saveProjectAs(callbacks);
      break;
    case k_closeProjectCommand:
      main_menu::closeProject(callbacks);
      break;
    case k_loadLayoutsCommand:
      main_menu::loadLayouts(callbacks);
      break;
    case k_saveLayoutsCommand:
      main_menu::saveLayouts(callbacks);
      break;
    case k_previousLayoutCommand:
      if (callbacks.cycleLayouts) {
        callbacks.cycleLayouts(-1);
      }
      break;
    case k_nextLayoutCommand:
      if (callbacks.cycleLayouts) {
        callbacks.cycleLayouts(1);
      }
      break;
    case k_showAboutCommand:
      if (callbacks.showAbout) {
        callbacks.showAbout();
      }
      break;
    case k_showKeyboardShortcutsCommand:
      if (callbacks.showKeyboardShortcuts) {
        callbacks.showKeyboardShortcuts();
      }
      break;
    case k_checkForUpdatesCommand:
      if (callbacks.checkForUpdates) {
        callbacks.checkForUpdates();
      }
      break;
    case k_openDownloadPageCommand:
      if (callbacks.openDownloadPage) {
        callbacks.openDownloadPage();
      }
      break;
    case k_clearRecentsCommand:
      if (callbacks.clearRecents) {
        callbacks.clearRecents();
      }
      break;
    case k_quitCommand:
      main_menu::quitApp(callbacks);
      break;
    default:
      break;
  }
}

LRESULT CALLBACK
entropyMenuSubclassProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam, UINT_PTR subclassId, DWORD_PTR refData)
{
  auto* state = reinterpret_cast<MenuState*>(refData);

  if (state && WM_COMMAND == message) {
    const UINT command = LOWORD(wParam);
    const bool fromMenu = (0 == HIWORD(wParam) && 0 == lParam);

    if (fromMenu && isMenuCommand(command)) {
      handleMenuCommand(*state, command);
      return 0;
    }
  }
  else if (state && (WM_INITMENU == message || WM_INITMENUPOPUP == message)) {
    updateEnabledState(*state);
  }
  else if (WM_NCDESTROY == message) {
    RemoveWindowSubclass(window, entropyMenuSubclassProc, subclassId);
    g_menuStates.erase(window);
  }

  return DefSubclassProc(window, message, wParam, lParam);
}

bool insertMenuItem(HMENU menu, UINT position, UINT command, const wchar_t* text)
{
  MENUITEMINFOW item{};
  item.cbSize = sizeof(item);
  item.fMask = MIIM_FTYPE | MIIM_ID | MIIM_STRING;
  item.fType = MFT_STRING;
  item.wID = command;
  item.dwTypeData = const_cast<wchar_t*>(text);
  return InsertMenuItemW(menu, position, TRUE, &item);
}

UINT actionCommand(MainMenuAction action)
{
  return k_actionCommandBase + static_cast<UINT>(action);
}

bool insertActionMenuItem(HMENU menu, UINT position, MainMenuAction action, const wchar_t* text)
{
  return insertMenuItem(menu, position, actionCommand(action), text);
}

bool insertSeparator(HMENU menu, UINT position)
{
  MENUITEMINFOW item{};
  item.cbSize = sizeof(item);
  item.fMask = MIIM_FTYPE;
  item.fType = MFT_SEPARATOR;
  return InsertMenuItemW(menu, position, TRUE, &item);
}

bool insertSubmenu(HMENU menu, UINT position, HMENU submenu, const wchar_t* text)
{
  MENUITEMINFOW item{};
  item.cbSize = sizeof(item);
  item.fMask = MIIM_FTYPE | MIIM_SUBMENU | MIIM_STRING;
  item.fType = MFT_STRING;
  item.hSubMenu = submenu;
  item.dwTypeData = const_cast<wchar_t*>(text);
  return InsertMenuItemW(menu, position, TRUE, &item);
}

bool insertDisabledMenuItem(HMENU menu, UINT position, const wchar_t* text)
{
  MENUITEMINFOW item{};
  item.cbSize = sizeof(item);
  item.fMask = MIIM_FTYPE | MIIM_STRING | MIIM_STATE;
  item.fType = MFT_STRING;
  item.fState = MFS_DISABLED;
  item.dwTypeData = const_cast<wchar_t*>(text);
  return InsertMenuItemW(menu, position, TRUE, &item);
}

bool clearMenu(HMENU menu)
{
  while (GetMenuItemCount(menu) > 0) {
    if (!DeleteMenu(menu, 0, MF_BYPOSITION)) {
      return false;
    }
  }
  return true;
}

bool populateRecentPathGroupsMenu(
  HMENU menu,
  MenuState& state,
  UINT commandBase,
  RecentCommandKind kind,
  const std::vector<std::vector<std::filesystem::path>>& groups)
{
  UINT position = 0;
  if (groups.empty()) {
    return insertDisabledMenuItem(menu, position, L"None");
  }

  for (std::size_t i = 0; i < groups.size() && i < k_recentCommandCount; ++i) {
    const UINT command = commandBase + static_cast<UINT>(i);
    const std::wstring label = recentPathLabel(groups.at(i));
    if (!insertMenuItem(menu, position++, command, label.c_str())) {
      return false;
    }
    state.recentCommands[command] = RecentCommand{.kind = kind, .paths = groups.at(i)};
  }
  return true;
}

bool populateRecentProjectsMenu(HMENU menu, MenuState& state, const std::vector<std::filesystem::path>& projects)
{
  std::vector<std::vector<std::filesystem::path>> groups;
  groups.reserve(projects.size());
  for (const auto& project : projects) {
    groups.push_back({project});
  }
  return populateRecentPathGroupsMenu(menu, state, k_recentProjectCommandBase, RecentCommandKind::Project, groups);
}

bool populateOpenRecentMenu(HMENU menu, MenuState& state)
{
  if (!clearMenu(menu)) {
    return false;
  }

  state.recentCommands.clear();

  HMENU projectsMenu = CreatePopupMenu();
  HMENU imagesMenu = CreatePopupMenu();
  HMENU imageGroupsMenu = CreatePopupMenu();
  HMENU dicomMenu = CreatePopupMenu();
  if (!projectsMenu || !imagesMenu || !imageGroupsMenu || !dicomMenu) {
    if (projectsMenu) DestroyMenu(projectsMenu);
    if (imagesMenu) DestroyMenu(imagesMenu);
    if (imageGroupsMenu) DestroyMenu(imageGroupsMenu);
    if (dicomMenu) DestroyMenu(dicomMenu);
    return false;
  }

  const auto projects =
    state.callbacks.recentProjectFiles ? state.callbacks.recentProjectFiles() : std::vector<std::filesystem::path>{};
  const auto imageGroups = state.callbacks.recentImageGroups ? state.callbacks.recentImageGroups()
                                                             : std::vector<std::vector<std::filesystem::path>>{};
  const auto dicomGroups = state.callbacks.recentDicomGroups ? state.callbacks.recentDicomGroups()
                                                             : std::vector<std::vector<std::filesystem::path>>{};
  state.recentMenuSignature = buildRecentMenuSignature(state.callbacks);

  std::vector<std::vector<std::filesystem::path>> singleImages;
  std::vector<std::vector<std::filesystem::path>> multiImageGroups;
  for (const auto& group : imageGroups) {
    if (group.empty()) {
      continue;
    }
    if (group.size() == 1) {
      singleImages.push_back(group);
    }
    else {
      multiImageGroups.push_back(group);
    }
  }

  if (
    !populateRecentProjectsMenu(projectsMenu, state, projects) ||
    !populateRecentPathGroupsMenu(
      imagesMenu,
      state,
      k_recentImageCommandBase,
      RecentCommandKind::ImageGroup,
      singleImages) ||
    !populateRecentPathGroupsMenu(
      imageGroupsMenu,
      state,
      k_recentImageGroupCommandBase,
      RecentCommandKind::ImageGroup,
      multiImageGroups) ||
    !populateRecentPathGroupsMenu(
      dicomMenu,
      state,
      k_recentDicomCommandBase,
      RecentCommandKind::DicomGroup,
      dicomGroups))
  {
    DestroyMenu(projectsMenu);
    DestroyMenu(imagesMenu);
    DestroyMenu(imageGroupsMenu);
    DestroyMenu(dicomMenu);
    return false;
  }

  UINT position = 0;
  return insertSubmenu(menu, position++, projectsMenu, L"&Projects") &&
         insertSubmenu(menu, position++, imagesMenu, L"&Images") &&
         insertSubmenu(menu, position++, imageGroupsMenu, L"Image &Groups") &&
         insertSubmenu(menu, position++, dicomMenu, L"&DICOM Series") && insertSeparator(menu, position++) &&
         insertMenuItem(menu, position++, k_clearRecentsCommand, L"&Clear Recents");
}

bool populateFileMenu(HMENU fileMenu, HMENU openRecentMenu)
{
  UINT position = 0;
  return insertMenuItem(fileMenu, position++, k_openImageCommand, L"&Open Image(s)...\tCtrl+O") &&
         insertMenuItem(fileMenu, position++, k_openProjectCommand, L"Open &Project...\tCtrl+Shift+O") &&
         insertMenuItem(fileMenu, position++, k_openDicomSeriesCommand, L"Open &DICOM Series...") &&
         insertSubmenu(fileMenu, position++, openRecentMenu, L"Open &Recent") &&
         insertSeparator(fileMenu, position++) &&
         insertMenuItem(fileMenu, position++, k_addImageCommand, L"&Add Image(s)...") &&
         insertSeparator(fileMenu, position++) &&
         insertMenuItem(fileMenu, position++, k_saveProjectCommand, L"&Save Project\tCtrl+S") &&
         insertMenuItem(fileMenu, position++, k_saveProjectAsCommand, L"Save Project &As...\tCtrl+Shift+S") &&
         insertActionMenuItem(
           fileMenu,
           position++,
           MainMenuAction::ResetProjectSettings,
           L"Reset Project &Settings...") &&
         insertSeparator(fileMenu, position++) &&
         insertMenuItem(fileMenu, position++, k_closeProjectCommand, L"&Close Project") &&
         insertSeparator(fileMenu, position++) && insertMenuItem(fileMenu, position++, k_quitCommand, L"&Quit\tCtrl+Q");
}

bool populateModesMenu(HMENU menu)
{
  UINT position = 0;
  return insertActionMenuItem(menu, position++, MainMenuAction::SetModePointer, L"&Pointer\tV") &&
         insertActionMenuItem(menu, position++, MainMenuAction::SetModeWindowLevel, L"&Window / Level\tL") &&
         insertActionMenuItem(menu, position++, MainMenuAction::SetModeZoom, L"&Zoom\tZ") &&
         insertActionMenuItem(menu, position++, MainMenuAction::SetModePan, L"&Pan / Dolly\tP") &&
         insertActionMenuItem(menu, position++, MainMenuAction::SetModeRotateView, L"&Rotate View") &&
         insertActionMenuItem(menu, position++, MainMenuAction::SetModeRotateCrosshairs, L"Rotate Cross&hairs") &&
         insertSeparator(menu, position++) &&
         insertActionMenuItem(menu, position++, MainMenuAction::SetModeSegment, L"Segmentation &Brush\tB") &&
         insertActionMenuItem(menu, position++, MainMenuAction::SetModeAnnotate, L"Vector &Annotate") &&
         insertSeparator(menu, position++) &&
         insertActionMenuItem(menu, position++, MainMenuAction::SetModeTranslateImage, L"&Translate Image\tT") &&
         insertActionMenuItem(menu, position++, MainMenuAction::SetModeRotateImage, L"R&otate Image\tR") &&
         insertActionMenuItem(menu, position++, MainMenuAction::SetModeScaleImage, L"&Scale Image\tY");
}

bool populateImageMenu(HMENU menu, HMENU activeImagesMenu)
{
  HMENU affineMenu = CreatePopupMenu();
  if (!affineMenu) {
    return false;
  }
  HMENU deformationMenu = CreatePopupMenu();
  if (!deformationMenu) {
    DestroyMenu(affineMenu);
    return false;
  }
  HMENU isosurfacesMenu = CreatePopupMenu();
  if (!isosurfacesMenu) {
    DestroyMenu(affineMenu);
    DestroyMenu(deformationMenu);
    return false;
  }
  HMENU timeSeriesMenu = CreatePopupMenu();
  if (!timeSeriesMenu) {
    DestroyMenu(affineMenu);
    DestroyMenu(deformationMenu);
    DestroyMenu(isosurfacesMenu);
    return false;
  }
  HMENU registrationMenu = CreatePopupMenu();
  if (!registrationMenu) {
    DestroyMenu(affineMenu);
    DestroyMenu(deformationMenu);
    DestroyMenu(isosurfacesMenu);
    DestroyMenu(timeSeriesMenu);
    return false;
  }

  UINT timeSeriesPosition = 0;
  if (
    !insertActionMenuItem(
      timeSeriesMenu,
      timeSeriesPosition++,
      MainMenuAction::ToggleGlobalTimeControls,
      L"Show &Time Controls") ||
    !insertSeparator(timeSeriesMenu, timeSeriesPosition++) ||
    !insertActionMenuItem(timeSeriesMenu, timeSeriesPosition++, MainMenuAction::ToggleTimePlayback, L"&Play / Pause") ||
    !insertSeparator(timeSeriesMenu, timeSeriesPosition++) ||
    !insertActionMenuItem(timeSeriesMenu, timeSeriesPosition++, MainMenuAction::FirstTimePoint, L"&First Frame") ||
    !insertActionMenuItem(
      timeSeriesMenu,
      timeSeriesPosition++,
      MainMenuAction::PreviousTimePoint,
      L"&Previous Frame\tAlt+,") ||
    !insertActionMenuItem(timeSeriesMenu, timeSeriesPosition++, MainMenuAction::NextTimePoint, L"&Next Frame\tAlt+.") ||
    !insertActionMenuItem(timeSeriesMenu, timeSeriesPosition++, MainMenuAction::LastTimePoint, L"&Last Frame"))
  {
    DestroyMenu(affineMenu);
    DestroyMenu(deformationMenu);
    DestroyMenu(isosurfacesMenu);
    DestroyMenu(timeSeriesMenu);
    DestroyMenu(registrationMenu);
    return false;
  }

  UINT affinePosition = 0;
  if (
    !insertActionMenuItem(
      affineMenu,
      affinePosition++,
      MainMenuAction::LoadActiveImageInitialTransformation,
      L"&Load Initial Affine...") ||
    !insertActionMenuItem(
      affineMenu,
      affinePosition++,
      MainMenuAction::SaveActiveImageInitialTransformation,
      L"Save &Initial &Affine...") ||
    !insertActionMenuItem(
      affineMenu,
      affinePosition++,
      MainMenuAction::ResetActiveImageInitialTransformation,
      L"Reset Initial Affine") ||
    !insertSeparator(affineMenu, affinePosition++) ||
    !insertActionMenuItem(
      affineMenu,
      affinePosition++,
      MainMenuAction::SaveActiveImageManualTransformation,
      L"Save &Manual Affine...") ||
    !insertActionMenuItem(
      affineMenu,
      affinePosition++,
      MainMenuAction::ResetActiveImageManualTransformation,
      L"&Reset Manual Affine") ||
    !insertSeparator(affineMenu, affinePosition++) ||
    !insertActionMenuItem(
      affineMenu,
      affinePosition++,
      MainMenuAction::SaveActiveImageEffectiveTransformation,
      L"Save &Effective (Manual * Initial) Affine..."))
  {
    DestroyMenu(affineMenu);
    DestroyMenu(deformationMenu);
    DestroyMenu(isosurfacesMenu);
    DestroyMenu(timeSeriesMenu);
    DestroyMenu(registrationMenu);
    return false;
  }

  UINT deformationPosition = 0;
  if (
    !insertMenuItem(deformationMenu, deformationPosition++, k_loadInverseWarpCommand, L"Load &Inverse Warp Field...") ||
    !insertMenuItem(deformationMenu, deformationPosition++, k_loadForwardWarpCommand, L"Load &Forward Warp Field...") ||
    !insertActionMenuItem(
      deformationMenu,
      deformationPosition++,
      MainMenuAction::ToggleApplyActiveImageWarp,
      L"&Apply Deformable Warp"))
  {
    DestroyMenu(affineMenu);
    DestroyMenu(deformationMenu);
    DestroyMenu(isosurfacesMenu);
    DestroyMenu(timeSeriesMenu);
    DestroyMenu(registrationMenu);
    return false;
  }

  UINT registrationPosition = 0;
  if (
    !insertActionMenuItem(
      registrationMenu,
      registrationPosition++,
      MainMenuAction::ShowRegistrationSetupWindow,
      L"&Registration...") ||
    !insertActionMenuItem(
      registrationMenu,
      registrationPosition++,
      MainMenuAction::ToggleRegistrationJobsWindow,
      L"Image Registration &Jobs..."))
  {
    DestroyMenu(affineMenu);
    DestroyMenu(deformationMenu);
    DestroyMenu(isosurfacesMenu);
    DestroyMenu(timeSeriesMenu);
    DestroyMenu(registrationMenu);
    return false;
  }

  UINT isosurfacesPosition = 0;
  if (
    !insertActionMenuItem(
      isosurfacesMenu,
      isosurfacesPosition++,
      MainMenuAction::ToggleIsosurfacesWindow,
      L"I&sosurfaces Panel") ||
    !insertSeparator(isosurfacesMenu, isosurfacesPosition++) ||
    !insertActionMenuItem(
      isosurfacesMenu,
      isosurfacesPosition++,
      MainMenuAction::AddIsosurface,
      L"Add I&sosurface...") ||
    !insertActionMenuItem(
      isosurfacesMenu,
      isosurfacesPosition++,
      MainMenuAction::AddIsosurfaceRange,
      L"Add Isosurface &Range...") ||
    !insertActionMenuItem(
      isosurfacesMenu,
      isosurfacesPosition++,
      MainMenuAction::ToggleActiveImageIsosurfaces,
      L"&Show Isosurfaces"))
  {
    DestroyMenu(affineMenu);
    DestroyMenu(deformationMenu);
    DestroyMenu(isosurfacesMenu);
    DestroyMenu(timeSeriesMenu);
    return false;
  }

  UINT position = 0;
  const bool ok =
    insertActionMenuItem(menu, position++, MainMenuAction::ToggleImagesWindow, L"&Images Panel") &&
    insertSeparator(menu, position++) && insertMenuItem(menu, position++, k_addImageCommand, L"&Add Image(s)...") &&
    insertMenuItem(menu, position++, k_addDicomSeriesCommand, L"Add &DICOM Series...") &&
    insertActionMenuItem(menu, position++, MainMenuAction::ExportActiveImage, L"&Export DICOM Series as Image...") &&
    insertSeparator(menu, position++) && insertSubmenu(menu, position++, activeImagesMenu, L"&Active Image") &&
    insertActionMenuItem(menu, position++, MainMenuAction::RemoveActiveImage, L"&Remove Active Image") &&
    insertActionMenuItem(menu, position++, MainMenuAction::SetActiveImageAsReference, L"Set Image as &Reference") &&
    insertActionMenuItem(
      menu,
      position++,
      MainMenuAction::ToggleActiveImageTransformationLock,
      L"&Lock Manual Affine Transformation") &&
    insertSeparator(menu, position++) &&
    insertActionMenuItem(menu, position++, MainMenuAction::MoveActiveImageBackward, L"Move Image &Backward") &&
    insertActionMenuItem(menu, position++, MainMenuAction::MoveActiveImageForward, L"Move Image &Forward") &&
    insertActionMenuItem(menu, position++, MainMenuAction::MoveActiveImageToBack, L"Move Image to Bac&k") &&
    insertActionMenuItem(menu, position++, MainMenuAction::MoveActiveImageToFront, L"Move Image to Fron&t") &&
    insertSeparator(menu, position++) && insertSubmenu(menu, position++, timeSeriesMenu, L"&Time Series") &&
    insertSubmenu(menu, position++, isosurfacesMenu, L"I&sosurfaces") && insertSeparator(menu, position++) &&
    insertSubmenu(menu, position++, affineMenu, L"&Affine Transformations") &&
    insertSubmenu(menu, position++, deformationMenu, L"&Deformable Transformations") &&
    insertSubmenu(menu, position++, registrationMenu, L"&Image Registration");

  if (!ok) {
    DestroyMenu(affineMenu);
    DestroyMenu(deformationMenu);
    DestroyMenu(isosurfacesMenu);
    DestroyMenu(timeSeriesMenu);
    DestroyMenu(registrationMenu);
  }
  return ok;
}

bool populateSegmentationMenu(HMENU menu)
{
  UINT position = 0;
  return insertActionMenuItem(menu, position++, MainMenuAction::ToggleSegmentationsWindow, L"Se&gmentations Panel") &&
         insertSeparator(menu, position++) &&
         insertMenuItem(menu, position++, k_addSegmentationCommand, L"&Add Segmentation...") &&
         insertActionMenuItem(menu, position++, MainMenuAction::CreateSegmentation, L"&Create Blank Segmentation") &&
         insertActionMenuItem(menu, position++, MainMenuAction::SaveSegmentation, L"&Save Active Segmentation...") &&
         insertActionMenuItem(menu, position++, MainMenuAction::ClearSegmentation, L"C&lear Active Segmentation") &&
         insertActionMenuItem(menu, position++, MainMenuAction::RemoveSegmentation, L"&Remove Active Segmentation") &&
         insertSeparator(menu, position++) &&
         insertActionMenuItem(
           menu,
           position++,
           MainMenuAction::PreviousForegroundLabel,
           L"Previous Foreground Label\t,") &&
         insertActionMenuItem(menu, position++, MainMenuAction::NextForegroundLabel, L"Next Foreground Label\t.") &&
         insertActionMenuItem(
           menu,
           position++,
           MainMenuAction::PreviousBackgroundLabel,
           L"Previous Background Label\t<") &&
         insertActionMenuItem(menu, position++, MainMenuAction::NextBackgroundLabel, L"Next Background Label\t>") &&
         insertSeparator(menu, position++) &&
         insertActionMenuItem(menu, position++, MainMenuAction::DecreaseBrushSize, L"Decrease Brush Size\t-") &&
         insertActionMenuItem(menu, position++, MainMenuAction::IncreaseBrushSize, L"Increase Brush Size\t+");
}

bool populateAnnotationsMenu(HMENU menu)
{
  UINT position = 0;
  return insertActionMenuItem(menu, position++, MainMenuAction::ToggleAnnotationsWindow, L"&Annotations Panel") &&
         insertSeparator(menu, position++) &&
         insertActionMenuItem(menu, position++, MainMenuAction::SaveAnnotations, L"&Save All Annotations...") &&
         insertActionMenuItem(menu, position++, MainMenuAction::RemoveAnnotation, L"&Remove Active Annotation") &&
         insertSeparator(menu, position++) &&
         insertActionMenuItem(menu, position++, MainMenuAction::MoveAnnotationBackward, L"Move Annotation &Backward") &&
         insertActionMenuItem(menu, position++, MainMenuAction::MoveAnnotationForward, L"Move Annotation &Forward") &&
         insertActionMenuItem(menu, position++, MainMenuAction::MoveAnnotationToBack, L"Move Annotation to Bac&k") &&
         insertActionMenuItem(menu, position++, MainMenuAction::MoveAnnotationToFront, L"Move Annotation to Fron&t") &&
         insertSeparator(menu, position++) &&
         insertActionMenuItem(
           menu,
           position++,
           MainMenuAction::PaintSegmentationFromAnnotation,
           L"&Paint Segmentation from Annotation");
}

bool populateLandmarksMenu(HMENU menu)
{
  UINT position = 0;
  return insertActionMenuItem(menu, position++, MainMenuAction::ToggleLandmarksWindow, L"&Landmarks Panel") &&
         insertSeparator(menu, position++) &&
         insertActionMenuItem(menu, position++, MainMenuAction::CreateLandmarkGroup, L"&Create Landmark Group") &&
         insertActionMenuItem(menu, position++, MainMenuAction::SaveLandmarkGroup, L"&Save Active Landmark Group...") &&
         insertActionMenuItem(menu, position++, MainMenuAction::AddLandmark, L"&Add Landmark at Crosshairs");
}

bool populateViewsMenu(HMENU menu)
{
  HMENU syncMenu = CreatePopupMenu();
  if (!syncMenu) {
    return false;
  }

  UINT syncPosition = 0;
  if (
    !insertActionMenuItem(syncMenu, syncPosition++, MainMenuAction::ToggleSync, L"&Enable Synchronization") ||
    !insertSeparator(syncMenu, syncPosition++) ||
    !insertActionMenuItem(syncMenu, syncPosition++, MainMenuAction::ToggleSyncSendCursor, L"Cursor: &Send") ||
    !insertActionMenuItem(syncMenu, syncPosition++, MainMenuAction::ToggleSyncReceiveCursor, L"Cursor: &Receive") ||
    !insertActionMenuItem(syncMenu, syncPosition++, MainMenuAction::ToggleSyncSendZoom, L"Zoom: Send") ||
    !insertActionMenuItem(syncMenu, syncPosition++, MainMenuAction::ToggleSyncReceiveZoom, L"Zoom: Receive") ||
    !insertActionMenuItem(syncMenu, syncPosition++, MainMenuAction::ToggleSyncSendPan, L"Pan: Send") ||
    !insertActionMenuItem(syncMenu, syncPosition++, MainMenuAction::ToggleSyncReceivePan, L"Pan: Receive") ||
    !insertSeparator(syncMenu, syncPosition++) ||
    !insertActionMenuItem(syncMenu, syncPosition++, MainMenuAction::ShowSynchronizeSettingsWindow, L"Settings..."))
  {
    DestroyMenu(syncMenu);
    return false;
  }

  UINT position = 0;
  return insertActionMenuItem(menu, position++, MainMenuAction::Recenter, L"&Recenter Views\tC") &&
         insertActionMenuItem(menu, position++, MainMenuAction::ResetView, L"Reset Views and &Crosshairs\tShift+C") &&
         insertSeparator(menu, position++) &&
         insertActionMenuItem(menu, position++, MainMenuAction::ToggleImageVisibility, L"Show &Image\tW") &&
         insertActionMenuItem(menu, position++, MainMenuAction::ToggleImageEdges, L"Show Image &Edges\tShift+E") &&
         insertActionMenuItem(
           menu,
           position++,
           MainMenuAction::ToggleSegmentationVisibility,
           L"Show &Segmentation\tS") &&
         insertActionMenuItem(
           menu,
           position++,
           MainMenuAction::ToggleSegmentationOutline,
           L"Show Segmentation &Outline\tSpace") &&
         insertSeparator(menu, position++) &&
         insertActionMenuItem(
           menu,
           position++,
           MainMenuAction::DecreaseActiveImageOpacity,
           L"Decrease Active Image Opacity\tQ") &&
         insertActionMenuItem(
           menu,
           position++,
           MainMenuAction::IncreaseActiveImageOpacity,
           L"Increase Active Image Opacity\tE") &&
         insertActionMenuItem(
           menu,
           position++,
           MainMenuAction::DecreaseSegmentationOpacity,
           L"Decrease Segmentation Opacity\tA") &&
         insertActionMenuItem(
           menu,
           position++,
           MainMenuAction::IncreaseSegmentationOpacity,
           L"Increase Segmentation Opacity\tD") &&
         insertSeparator(menu, position++) &&
         insertActionMenuItem(menu, position++, MainMenuAction::ToggleCrosshairs, L"Show Crosshairs\tX") &&
         insertActionMenuItem(
           menu,
           position++,
           MainMenuAction::ToggleCrosshairsVoxelSnapping,
           L"Snap Crosshairs to &Voxels") &&
         insertActionMenuItem(menu, position++, MainMenuAction::ToggleScaleBars, L"Show Scale &Bars") &&
         insertActionMenuItem(menu, position++, MainMenuAction::ToggleUserInterface, L"Show User Interface\tU") &&
         insertActionMenuItem(menu, position++, MainMenuAction::CycleViewOverlays, L"Cycle View Overlays\tO") &&
         insertSeparator(menu, position++) &&
         insertActionMenuItem(menu, position++, MainMenuAction::ToggleAsciiRendering, L"&ASCII Rendering") &&
         insertSeparator(menu, position++) &&
         insertActionMenuItem(menu, position++, MainMenuAction::ToggleFullScreen, L"&Enter Full Screen\tF4") &&
         insertSeparator(menu, position++) &&
         insertActionMenuItem(
           menu,
           position++,
           MainMenuAction::ToggleEntropyInstanceSync,
           L"Synchronize Entropy Instances") &&
         insertSubmenu(menu, position++, syncMenu, L"Synchronize with &ITK-SNAP");
}

bool populateWindowsMenu(HMENU menu)
{
  UINT position = 0;
  bool ok =
    insertActionMenuItem(menu, position++, MainMenuAction::ToggleImagesWindow, L"&Images Panel") &&
    insertActionMenuItem(menu, position++, MainMenuAction::ToggleSegmentationsWindow, L"&Segmentations Panel") &&
    insertActionMenuItem(menu, position++, MainMenuAction::ShowRegistrationSetupWindow, L"&Registration Panel") &&
    insertActionMenuItem(menu, position++, MainMenuAction::ToggleAnnotationsWindow, L"&Annotations Panel") &&
    insertActionMenuItem(menu, position++, MainMenuAction::ToggleLandmarksWindow, L"&Landmarks Panel") &&
    insertActionMenuItem(menu, position++, MainMenuAction::ToggleIsosurfacesWindow, L"I&sosurfaces Panel") &&
    insertActionMenuItem(menu, position++, MainMenuAction::ResetPanelLayout, L"Reset Panel &Layout") &&
    insertSeparator(menu, position++) &&
    insertActionMenuItem(menu, position++, MainMenuAction::ToggleInspectorWindow, L"Voxel Ins&pector\tI") &&
    insertActionMenuItem(menu, position++, MainMenuAction::ToggleOpacityMixerWindow, L"&Opacity Mixer") &&
    insertActionMenuItem(menu, position++, MainMenuAction::ToggleRegistrationJobsWindow, L"Registration &Jobs") &&
    insertActionMenuItem(menu, position++, MainMenuAction::ToggleSettingsWindow, L"Application Se&ttings\tCtrl+,") &&
    insertSeparator(menu, position++) &&
    insertActionMenuItem(menu, position++, MainMenuAction::ToggleToolbar, L"T&oolbar");
#ifndef NDEBUG
  ok = ok && insertActionMenuItem(menu, position++, MainMenuAction::ToggleImGuiDemoWindow, L"ImGui &Demo") &&
       insertActionMenuItem(menu, position++, MainMenuAction::ToggleImPlotDemoWindow, L"ImPlot Demo");
#endif
  return ok;
}

bool populateHelpMenu(HMENU menu)
{
  UINT position = 0;
  return insertMenuItem(menu, position++, k_checkForUpdatesCommand, L"Check for &Updates...") &&
         insertMenuItem(menu, position++, k_openDownloadPageCommand, L"&Download Entropy...") &&
         insertSeparator(menu, position++) &&
         insertMenuItem(menu, position++, k_showKeyboardShortcutsCommand, L"&Keyboard Shortcuts") &&
         insertSeparator(menu, position++) && insertMenuItem(menu, position++, k_showAboutCommand, L"&About Entropy");
}

bool populateLayoutsMenu(HMENU layoutsMenu, const MainMenuBarCallbacks& callbacks)
{
  if (!clearMenu(layoutsMenu)) {
    return false;
  }

  UINT position = 0;
  if (
    !insertMenuItem(layoutsMenu, position++, k_loadLayoutsCommand, L"&Load Layouts...") ||
    !insertMenuItem(layoutsMenu, position++, k_saveLayoutsCommand, L"&Save Layouts...") ||
    !insertSeparator(layoutsMenu, position++) ||
    !insertActionMenuItem(layoutsMenu, position++, MainMenuAction::AddLayout, L"&Add Layout...") ||
    !insertActionMenuItem(layoutsMenu, position++, MainMenuAction::RemoveLayout, L"&Remove Current Layout") ||
    !insertActionMenuItem(layoutsMenu, position++, MainMenuAction::ToggleLayoutTabs, L"Show Layout &Tabs") ||
    !insertSeparator(layoutsMenu, position++) ||
    !insertMenuItem(layoutsMenu, position++, k_previousLayoutCommand, L"&Previous Layout\t[") ||
    !insertMenuItem(layoutsMenu, position++, k_nextLayoutCommand, L"&Next Layout\t]") ||
    !insertSeparator(layoutsMenu, position++))
  {
    return false;
  }

  const auto layoutNames = callbacks.layoutNames ? callbacks.layoutNames() : std::vector<std::string>{};
  const std::size_t currentIndex = callbacks.currentLayoutIndex ? callbacks.currentLayoutIndex() : 0;
  for (std::size_t i = 0; i < layoutNames.size(); ++i) {
    const std::wstring title = widenUtf8(layoutNames.at(i));
    const UINT command = k_selectLayoutCommandBase + static_cast<UINT>(i);
    if (!insertMenuItem(layoutsMenu, position++, command, title.c_str())) {
      return false;
    }
    CheckMenuItem(layoutsMenu, command, MF_BYCOMMAND | (i == currentIndex ? MF_CHECKED : MF_UNCHECKED));
  }

  return true;
}

bool populateActiveImagesMenu(HMENU activeImagesMenu, const MainMenuBarCallbacks& callbacks)
{
  if (!clearMenu(activeImagesMenu)) {
    return false;
  }

  UINT position = 0;
  const auto imageNames = callbacks.imageNames ? callbacks.imageNames() : std::vector<std::string>{};
  const std::size_t activeIndex = callbacks.activeImageIndex ? callbacks.activeImageIndex() : 0;
  for (std::size_t i = 0; i < imageNames.size(); ++i) {
    const std::wstring title = widenUtf8(imageNames.at(i));
    const UINT command = k_selectActiveImageCommandBase + static_cast<UINT>(i);
    if (!insertMenuItem(activeImagesMenu, position++, command, title.c_str())) {
      return false;
    }
    CheckMenuItem(activeImagesMenu, command, MF_BYCOMMAND | (i == activeIndex ? MF_CHECKED : MF_UNCHECKED));
  }
  return true;
}

bool installWindowsNativeMainMenu(HWND window, const MainMenuBarCallbacks& callbacks)
{
  if (!window || g_menuStates.contains(window)) {
    return false;
  }

  auto state = std::make_unique<MenuState>();
  state->callbacks = callbacks;

  state->mainMenu = CreateMenu();
  HMENU fileMenu = CreatePopupMenu();
  HMENU modesMenu = CreatePopupMenu();
  HMENU imageMenu = CreatePopupMenu();
  HMENU segmentationMenu = CreatePopupMenu();
  HMENU annotationsMenu = CreatePopupMenu();
  HMENU landmarksMenu = CreatePopupMenu();
  state->layoutsMenu = CreatePopupMenu();
  HMENU viewsMenu = CreatePopupMenu();
  HMENU windowsMenu = CreatePopupMenu();
  HMENU helpMenu = CreatePopupMenu();
  state->activeImagesMenu = CreatePopupMenu();
  state->openRecentMenu = CreatePopupMenu();
  if (
    !state->mainMenu || !fileMenu || !modesMenu || !imageMenu || !segmentationMenu || !annotationsMenu ||
    !landmarksMenu || !state->layoutsMenu || !viewsMenu || !windowsMenu || !helpMenu || !state->activeImagesMenu ||
    !state->openRecentMenu)
  {
    if (state->mainMenu) {
      DestroyMenu(state->mainMenu);
    }
    return false;
  }

  if (
    !populateOpenRecentMenu(state->openRecentMenu, *state) || !populateFileMenu(fileMenu, state->openRecentMenu) ||
    !populateModesMenu(modesMenu) || !populateActiveImagesMenu(state->activeImagesMenu, callbacks) ||
    !populateImageMenu(imageMenu, state->activeImagesMenu) || !populateSegmentationMenu(segmentationMenu) ||
    !populateAnnotationsMenu(annotationsMenu) || !populateLandmarksMenu(landmarksMenu) ||
    !populateLayoutsMenu(state->layoutsMenu, callbacks) || !populateViewsMenu(viewsMenu) ||
    !populateWindowsMenu(windowsMenu) || !populateHelpMenu(helpMenu))
  {
    DestroyMenu(state->mainMenu);
    return false;
  }

  if (
    !insertSubmenu(state->mainMenu, 0, fileMenu, L"&File") || !insertSubmenu(state->mainMenu, 1, modesMenu, L"&Mode") ||
    !insertSubmenu(state->mainMenu, 2, imageMenu, L"&Image") ||
    !insertSubmenu(state->mainMenu, 3, segmentationMenu, L"&Segmentation") ||
    !insertSubmenu(state->mainMenu, 4, annotationsMenu, L"&Annotation") ||
    !insertSubmenu(state->mainMenu, 5, landmarksMenu, L"Land&marks") ||
    !insertSubmenu(state->mainMenu, 6, state->layoutsMenu, L"&Layout") ||
    !insertSubmenu(state->mainMenu, 7, viewsMenu, L"&View") ||
    !insertSubmenu(state->mainMenu, 8, windowsMenu, L"&Window") ||
    !insertSubmenu(state->mainMenu, 9, helpMenu, L"&Help"))
  {
    DestroyMenu(state->mainMenu);
    return false;
  }

  if (!SetWindowSubclass(window, entropyMenuSubclassProc, k_menuSubclassId, reinterpret_cast<DWORD_PTR>(state.get()))) {
    DestroyMenu(state->mainMenu);
    return false;
  }

  if (!SetMenu(window, state->mainMenu)) {
    RemoveWindowSubclass(window, entropyMenuSubclassProc, k_menuSubclassId);
    DestroyMenu(state->mainMenu);
    return false;
  }

  updateEnabledState(*state);
  DrawMenuBar(window);

  g_menuStates.emplace(window, std::move(state));
  return true;
}
} // namespace

void updateWindowsNativeMainMenu(GLFWwindow* window, const MainMenuBarCallbacks& callbacks)
{
  const HWND hwnd = hwndFromGlfw(window);
  if (!hwnd) {
    return;
  }

  auto stateIt = g_menuStates.find(hwnd);
  if (std::end(g_menuStates) == stateIt) {
    installWindowsNativeMainMenu(hwnd, callbacks);
    return;
  }

  stateIt->second->callbacks = callbacks;
  const std::string recentSignature = buildRecentMenuSignature(callbacks);
  if (recentSignature != stateIt->second->recentMenuSignature) {
    populateOpenRecentMenu(stateIt->second->openRecentMenu, *stateIt->second);
  }
  populateLayoutsMenu(stateIt->second->layoutsMenu, callbacks);
  populateActiveImagesMenu(stateIt->second->activeImagesMenu, callbacks);
  updateEnabledState(*stateIt->second);
}

void uninstallWindowsNativeMainMenu(GLFWwindow* window)
{
  const HWND hwnd = hwndFromGlfw(window);
  if (!hwnd) {
    return;
  }

  auto stateIt = g_menuStates.find(hwnd);
  if (std::end(g_menuStates) == stateIt) {
    return;
  }

  HMENU menu = stateIt->second->mainMenu;
  RemoveWindowSubclass(hwnd, entropyMenuSubclassProc, k_menuSubclassId);
  SetMenu(hwnd, nullptr);
  if (menu) {
    DestroyMenu(menu);
  }
  DrawMenuBar(hwnd);
  g_menuStates.erase(stateIt);
}
