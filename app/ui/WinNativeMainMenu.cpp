#include "ui/WinNativeMainMenu.h"

#include <cstdint>

#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>
#include <windows.h>

namespace
{
constexpr UINT_PTR sk_openImageCommand = 1001;
constexpr UINT_PTR sk_openProjectCommand = 1002;
constexpr UINT_PTR sk_addImageCommand = 1003;
constexpr UINT_PTR sk_addSegmentationCommand = 1004;
constexpr UINT_PTR sk_saveProjectCommand = 1005;
constexpr UINT_PTR sk_saveProjectAsCommand = 1006;
constexpr UINT_PTR sk_closeProjectCommand = 1007;

MainMenuBarCallbacks g_callbacks;
HWND g_window = nullptr;
HMENU g_mainMenu = nullptr;
WNDPROC g_previousWindowProc = nullptr;
bool g_installed = false;

void enableMenuCommand(UINT_PTR command, bool enabled)
{
  if (!g_mainMenu) {
    return;
  }

  EnableMenuItem(g_mainMenu, static_cast<UINT>(command), MF_BYCOMMAND | (enabled ? MF_ENABLED : MF_GRAYED));
}

void updateEnabledState()
{
  enableMenuCommand(sk_openImageCommand, g_callbacks.canOpenProject && g_callbacks.openImageFile);
  enableMenuCommand(sk_openProjectCommand, g_callbacks.canOpenProject && g_callbacks.openProjectFile);
  enableMenuCommand(sk_addImageCommand, g_callbacks.canAddImage && g_callbacks.addImageFile);
  enableMenuCommand(sk_addSegmentationCommand, g_callbacks.canAddSegmentation && g_callbacks.addSegmentationFile);
  enableMenuCommand(sk_saveProjectCommand, g_callbacks.canSaveProject && g_callbacks.saveProject);
  enableMenuCommand(sk_saveProjectAsCommand, g_callbacks.canSaveProject && g_callbacks.saveProjectAs);
  enableMenuCommand(sk_closeProjectCommand, g_callbacks.canCloseProject && g_callbacks.closeProject);
}

void handleMenuCommand(UINT_PTR command)
{
  switch (command) {
    case sk_openImageCommand:
      main_menu::openImage(g_callbacks);
      break;
    case sk_openProjectCommand:
      main_menu::openProject(g_callbacks);
      break;
    case sk_addImageCommand:
      main_menu::addImage(g_callbacks);
      break;
    case sk_addSegmentationCommand:
      main_menu::addSegmentation(g_callbacks);
      break;
    case sk_saveProjectCommand:
      main_menu::saveProject(g_callbacks);
      break;
    case sk_saveProjectAsCommand:
      main_menu::saveProjectAs(g_callbacks);
      break;
    case sk_closeProjectCommand:
      main_menu::closeProject(g_callbacks);
      break;
    default:
      break;
  }
}

LRESULT CALLBACK entropyWindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
  if (WM_COMMAND == message) {
    const UINT_PTR command = LOWORD(wParam);

    switch (command) {
      case sk_openImageCommand:
      case sk_openProjectCommand:
      case sk_addImageCommand:
      case sk_addSegmentationCommand:
      case sk_saveProjectCommand:
      case sk_saveProjectAsCommand:
      case sk_closeProjectCommand:
        handleMenuCommand(command);
        return 0;
      default:
        break;
    }
  }
  else if (WM_INITMENUPOPUP == message) {
    updateEnabledState();
  }

  return CallWindowProc(g_previousWindowProc, window, message, wParam, lParam);
}

void appendMenuItem(HMENU menu, UINT_PTR command, const wchar_t* text)
{
  AppendMenuW(menu, MF_STRING, command, text);
}

void installWindowsNativeMainMenu(GLFWwindow* window)
{
  if (g_installed || !window) {
    return;
  }

  g_window = glfwGetWin32Window(window);
  if (!g_window) {
    return;
  }

  g_mainMenu = CreateMenu();
  HMENU fileMenu = CreatePopupMenu();
  if (!g_mainMenu || !fileMenu) {
    return;
  }

  appendMenuItem(fileMenu, sk_openImageCommand, L"Open Image...");
  appendMenuItem(fileMenu, sk_openProjectCommand, L"Open Project...");
  AppendMenuW(fileMenu, MF_SEPARATOR, 0, nullptr);
  appendMenuItem(fileMenu, sk_addImageCommand, L"Add Image...");
  appendMenuItem(fileMenu, sk_addSegmentationCommand, L"Add Segmentation...");
  AppendMenuW(fileMenu, MF_SEPARATOR, 0, nullptr);
  appendMenuItem(fileMenu, sk_saveProjectCommand, L"Save Project");
  appendMenuItem(fileMenu, sk_saveProjectAsCommand, L"Save Project As...");
  AppendMenuW(fileMenu, MF_SEPARATOR, 0, nullptr);
  appendMenuItem(fileMenu, sk_closeProjectCommand, L"Close Project");

  AppendMenuW(g_mainMenu, MF_POPUP, reinterpret_cast<UINT_PTR>(fileMenu), L"File");
  SetMenu(g_window, g_mainMenu);
  DrawMenuBar(g_window);

  SetLastError(0);
  const LONG_PTR previousWindowProc =
    SetWindowLongPtrW(g_window, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(entropyWindowProc));
  if (0 == previousWindowProc && 0 != GetLastError()) {
    return;
  }

  g_previousWindowProc = reinterpret_cast<WNDPROC>(previousWindowProc);
  g_installed = true;
}
} // namespace

void updateWindowsNativeMainMenu(GLFWwindow* window, const MainMenuBarCallbacks& callbacks)
{
  g_callbacks = callbacks;
  installWindowsNativeMainMenu(window);
  updateEnabledState();
}
