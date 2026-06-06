#include "ui/WinNativeMainMenu.h"

#include <memory>
#include <string>
#include <unordered_map>

#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>
#include <windows.h>
#include <commctrl.h>

namespace
{
constexpr UINT sk_openImageCommand = 1001;
constexpr UINT sk_openProjectCommand = 1002;
constexpr UINT sk_addImageCommand = 1003;
constexpr UINT sk_addSegmentationCommand = 1004;
constexpr UINT sk_saveProjectCommand = 1005;
constexpr UINT sk_saveProjectAsCommand = 1006;
constexpr UINT sk_closeProjectCommand = 1007;
constexpr UINT sk_loadLayoutsCommand = 1101;
constexpr UINT sk_saveLayoutsCommand = 1102;
constexpr UINT sk_previousLayoutCommand = 1103;
constexpr UINT sk_nextLayoutCommand = 1104;
constexpr UINT sk_selectLayoutCommandBase = 1200;

constexpr UINT_PTR sk_menuSubclassId = 1;

struct MenuState
{
  HMENU mainMenu = nullptr;
  HMENU layoutsMenu = nullptr;
  MainMenuBarCallbacks callbacks;
};

std::unordered_map<HWND, std::unique_ptr<MenuState>> g_menuStates;

HWND hwndFromGlfw(GLFWwindow* window)
{
  return window ? glfwGetWin32Window(window) : nullptr;
}

bool isMenuCommand(UINT command)
{
  if (command >= sk_selectLayoutCommandBase) {
    return true;
  }

  switch (command) {
    case sk_openImageCommand:
    case sk_openProjectCommand:
    case sk_addImageCommand:
    case sk_addSegmentationCommand:
    case sk_saveProjectCommand:
    case sk_saveProjectAsCommand:
    case sk_closeProjectCommand:
    case sk_loadLayoutsCommand:
    case sk_saveLayoutsCommand:
    case sk_previousLayoutCommand:
    case sk_nextLayoutCommand:
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

  enableMenuCommand(state, sk_openImageCommand, callbacks.canOpenProject && callbacks.openImageFile);
  enableMenuCommand(state, sk_openProjectCommand, callbacks.canOpenProject && callbacks.openProjectFile);
  enableMenuCommand(state, sk_addImageCommand, callbacks.canAddImage && callbacks.addImageFile);
  enableMenuCommand(state, sk_addSegmentationCommand, callbacks.canAddSegmentation && callbacks.addSegmentationFile);
  enableMenuCommand(state, sk_saveProjectCommand, callbacks.canSaveProject && callbacks.saveProject);
  enableMenuCommand(state, sk_saveProjectAsCommand, callbacks.canSaveProject && callbacks.saveProjectAs);
  enableMenuCommand(state, sk_closeProjectCommand, callbacks.canCloseProject && callbacks.closeProject);
  enableMenuCommand(state, sk_loadLayoutsCommand, callbacks.canUseLayouts && callbacks.loadLayoutsFile);
  enableMenuCommand(state, sk_saveLayoutsCommand, callbacks.canUseLayouts && callbacks.saveLayoutsFile);
  enableMenuCommand(state, sk_previousLayoutCommand, callbacks.canUseLayouts && callbacks.cycleLayouts);
  enableMenuCommand(state, sk_nextLayoutCommand, callbacks.canUseLayouts && callbacks.cycleLayouts);

  const auto layoutNames = callbacks.layoutNames ? callbacks.layoutNames() : std::vector<std::string>{};
  for (std::size_t i = 0; i < layoutNames.size(); ++i) {
    enableMenuCommand(
      state,
      sk_selectLayoutCommandBase + static_cast<UINT>(i),
      callbacks.canUseLayouts && callbacks.setCurrentLayoutIndex);
  }
}

void handleMenuCommand(const MenuState& state, UINT command)
{
  const auto& callbacks = state.callbacks;

  if (command >= sk_selectLayoutCommandBase) {
    if (callbacks.setCurrentLayoutIndex) {
      callbacks.setCurrentLayoutIndex(command - sk_selectLayoutCommandBase);
    }
    return;
  }

  switch (command) {
    case sk_openImageCommand:
      main_menu::openImage(callbacks);
      break;
    case sk_openProjectCommand:
      main_menu::openProject(callbacks);
      break;
    case sk_addImageCommand:
      main_menu::addImage(callbacks);
      break;
    case sk_addSegmentationCommand:
      main_menu::addSegmentation(callbacks);
      break;
    case sk_saveProjectCommand:
      main_menu::saveProject(callbacks);
      break;
    case sk_saveProjectAsCommand:
      main_menu::saveProjectAs(callbacks);
      break;
    case sk_closeProjectCommand:
      main_menu::closeProject(callbacks);
      break;
    case sk_loadLayoutsCommand:
      main_menu::loadLayouts(callbacks);
      break;
    case sk_saveLayoutsCommand:
      main_menu::saveLayouts(callbacks);
      break;
    case sk_previousLayoutCommand:
      if (callbacks.cycleLayouts) {
        callbacks.cycleLayouts(-1);
      }
      break;
    case sk_nextLayoutCommand:
      if (callbacks.cycleLayouts) {
        callbacks.cycleLayouts(1);
      }
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

bool populateFileMenu(HMENU fileMenu)
{
  UINT position = 0;
  return insertMenuItem(fileMenu, position++, sk_openImageCommand, L"&Open Image...") &&
         insertMenuItem(fileMenu, position++, sk_openProjectCommand, L"Open &Project...") &&
         insertSeparator(fileMenu, position++) &&
         insertMenuItem(fileMenu, position++, sk_addImageCommand, L"&Add Image...") &&
         insertMenuItem(fileMenu, position++, sk_addSegmentationCommand, L"Add &Segmentation...") &&
         insertSeparator(fileMenu, position++) &&
         insertMenuItem(fileMenu, position++, sk_saveProjectCommand, L"&Save Project") &&
         insertMenuItem(fileMenu, position++, sk_saveProjectAsCommand, L"Save Project &As...") &&
         insertSeparator(fileMenu, position++) &&
         insertMenuItem(fileMenu, position++, sk_closeProjectCommand, L"&Close Project");
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

bool populateLayoutsMenu(HMENU layoutsMenu, const MainMenuBarCallbacks& callbacks)
{
  if (!clearMenu(layoutsMenu)) {
    return false;
  }

  UINT position = 0;
  if (
    !insertMenuItem(layoutsMenu, position++, sk_loadLayoutsCommand, L"&Load...") ||
    !insertMenuItem(layoutsMenu, position++, sk_saveLayoutsCommand, L"&Save...") ||
    !insertSeparator(layoutsMenu, position++) ||
    !insertMenuItem(layoutsMenu, position++, sk_previousLayoutCommand, L"&Previous\t[") ||
    !insertMenuItem(layoutsMenu, position++, sk_nextLayoutCommand, L"&Next\t]") ||
    !insertSeparator(layoutsMenu, position++))
  {
    return false;
  }

  const auto layoutNames = callbacks.layoutNames ? callbacks.layoutNames() : std::vector<std::string>{};
  const std::size_t currentIndex = callbacks.currentLayoutIndex ? callbacks.currentLayoutIndex() : 0;
  for (std::size_t i = 0; i < layoutNames.size(); ++i) {
    const std::wstring title = widenUtf8(layoutNames.at(i));
    const UINT command = sk_selectLayoutCommandBase + static_cast<UINT>(i);
    if (!insertMenuItem(layoutsMenu, position++, command, title.c_str())) {
      return false;
    }
    CheckMenuItem(layoutsMenu, command, MF_BYCOMMAND | (i == currentIndex ? MF_CHECKED : MF_UNCHECKED));
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
  state->layoutsMenu = CreatePopupMenu();
  if (!state->mainMenu || !fileMenu || !state->layoutsMenu) {
    if (fileMenu) {
      DestroyMenu(fileMenu);
    }
    if (state->layoutsMenu) {
      DestroyMenu(state->layoutsMenu);
    }
    if (state->mainMenu) {
      DestroyMenu(state->mainMenu);
    }
    return false;
  }

  if (!populateFileMenu(fileMenu)) {
    DestroyMenu(fileMenu);
    DestroyMenu(state->layoutsMenu);
    DestroyMenu(state->mainMenu);
    return false;
  }

  if (!insertSubmenu(state->mainMenu, 0, fileMenu, L"&File")) {
    DestroyMenu(fileMenu);
    DestroyMenu(state->layoutsMenu);
    DestroyMenu(state->mainMenu);
    return false;
  }

  if (
    !populateLayoutsMenu(state->layoutsMenu, callbacks) ||
    !insertSubmenu(state->mainMenu, 1, state->layoutsMenu, L"&Layouts"))
  {
    DestroyMenu(state->layoutsMenu);
    DestroyMenu(state->mainMenu);
    return false;
  }

  if (!SetWindowSubclass(window, entropyMenuSubclassProc, sk_menuSubclassId, reinterpret_cast<DWORD_PTR>(state.get())))
  {
    DestroyMenu(state->mainMenu);
    return false;
  }

  if (!SetMenu(window, state->mainMenu)) {
    RemoveWindowSubclass(window, entropyMenuSubclassProc, sk_menuSubclassId);
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
  populateLayoutsMenu(stateIt->second->layoutsMenu, callbacks);
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
  RemoveWindowSubclass(hwnd, entropyMenuSubclassProc, sk_menuSubclassId);
  SetMenu(hwnd, nullptr);
  if (menu) {
    DestroyMenu(menu);
  }
  DrawMenuBar(hwnd);
  g_menuStates.erase(stateIt);
}
