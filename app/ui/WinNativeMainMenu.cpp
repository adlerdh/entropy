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
constexpr UINT sk_showAboutCommand = 1105;
constexpr UINT sk_selectLayoutCommandBase = 1200;
constexpr UINT sk_selectActiveImageCommandBase = 1400;
constexpr UINT sk_actionCommandBase = 2000;
constexpr UINT sk_actionCommandEnd = 2300;

constexpr UINT_PTR sk_menuSubclassId = 1;

struct MenuState
{
  HMENU mainMenu = nullptr;
  HMENU layoutsMenu = nullptr;
  HMENU activeImagesMenu = nullptr;
  MainMenuBarCallbacks callbacks;
};

std::unordered_map<HWND, std::unique_ptr<MenuState>> g_menuStates;

HWND hwndFromGlfw(GLFWwindow* window)
{
  return window ? glfwGetWin32Window(window) : nullptr;
}

bool isMenuCommand(UINT command)
{
  if (command >= sk_actionCommandBase && command < sk_actionCommandEnd) {
    return true;
  }

  if (command >= sk_selectLayoutCommandBase && command < sk_selectActiveImageCommandBase) {
    return true;
  }

  if (command >= sk_selectActiveImageCommandBase && command < sk_actionCommandBase) {
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
    case sk_showAboutCommand:
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

  const auto imageNames = callbacks.imageNames ? callbacks.imageNames() : std::vector<std::string>{};
  const std::size_t activeImageIndex = callbacks.activeImageIndex ? callbacks.activeImageIndex() : 0;
  for (std::size_t i = 0; i < imageNames.size(); ++i) {
    const UINT command = sk_selectActiveImageCommandBase + static_cast<UINT>(i);
    enableMenuCommand(state, command, callbacks.canAddImage && callbacks.setActiveImageIndex);
    CheckMenuItem(state.mainMenu, command, MF_BYCOMMAND | (i == activeImageIndex ? MF_CHECKED : MF_UNCHECKED));
  }

  for (UINT command = sk_actionCommandBase; command < sk_actionCommandEnd; ++command) {
    const auto action = static_cast<MainMenuAction>(command - sk_actionCommandBase);
    enableMenuCommand(state, command, main_menu::actionEnabled(callbacks, action));
    CheckMenuItem(
      state.mainMenu,
      command,
      MF_BYCOMMAND | (main_menu::actionChecked(callbacks, action) ? MF_CHECKED : MF_UNCHECKED));
  }
}

void handleMenuCommand(const MenuState& state, UINT command)
{
  const auto& callbacks = state.callbacks;

  if (command >= sk_actionCommandBase && command < sk_actionCommandEnd) {
    main_menu::performAction(callbacks, static_cast<MainMenuAction>(command - sk_actionCommandBase));
    return;
  }

  if (command >= sk_selectActiveImageCommandBase && command < sk_actionCommandBase) {
    if (callbacks.setActiveImageIndex) {
      callbacks.setActiveImageIndex(command - sk_selectActiveImageCommandBase);
    }
    return;
  }

  if (command >= sk_selectLayoutCommandBase && command < sk_selectActiveImageCommandBase) {
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
    case sk_showAboutCommand:
      if (callbacks.showAbout) {
        callbacks.showAbout();
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

UINT actionCommand(MainMenuAction action)
{
  return sk_actionCommandBase + static_cast<UINT>(action);
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

bool populateModesMenu(HMENU menu)
{
  UINT position = 0;
  return insertActionMenuItem(menu, position++, MainMenuAction::SetModePointer, L"&Pointer") &&
         insertActionMenuItem(menu, position++, MainMenuAction::SetModeWindowLevel, L"&Window / Level") &&
         insertActionMenuItem(menu, position++, MainMenuAction::SetModeZoom, L"&Zoom") &&
         insertActionMenuItem(menu, position++, MainMenuAction::SetModePan, L"&Pan / Dolly") &&
         insertActionMenuItem(menu, position++, MainMenuAction::SetModeRotateView, L"&Rotate View") &&
         insertActionMenuItem(menu, position++, MainMenuAction::SetModeRotateCrosshairs, L"Rotate Cross&hairs") &&
         insertSeparator(menu, position++) &&
         insertActionMenuItem(menu, position++, MainMenuAction::SetModeSegment, L"&Brush") &&
         insertActionMenuItem(menu, position++, MainMenuAction::SetModeAnnotate, L"&Annotate") &&
         insertSeparator(menu, position++) &&
         insertActionMenuItem(menu, position++, MainMenuAction::SetModeTranslateImage, L"&Translate Image") &&
         insertActionMenuItem(menu, position++, MainMenuAction::SetModeRotateImage, L"R&otate Image");
}

bool populateImageMenu(HMENU menu, HMENU activeImagesMenu)
{
  UINT position = 0;
  return insertActionMenuItem(menu, position++, MainMenuAction::ToggleImagesWindow, L"Show &Image Window") &&
         insertSeparator(menu, position++) && insertSubmenu(menu, position++, activeImagesMenu, L"&Active Image") &&
         insertSeparator(menu, position++) &&
         insertActionMenuItem(menu, position++, MainMenuAction::SetActiveImageAsReference, L"Set as &Reference") &&
         insertActionMenuItem(menu, position++, MainMenuAction::RemoveActiveImage, L"&Remove Image") &&
         insertSeparator(menu, position++) &&
         insertActionMenuItem(menu, position++, MainMenuAction::MoveActiveImageBackward, L"Move &Backward") &&
         insertActionMenuItem(menu, position++, MainMenuAction::MoveActiveImageForward, L"Move &Forward") &&
         insertActionMenuItem(menu, position++, MainMenuAction::MoveActiveImageToBack, L"Move to Bac&k") &&
         insertActionMenuItem(menu, position++, MainMenuAction::MoveActiveImageToFront, L"Move to Fron&t") &&
         insertSeparator(menu, position++) &&
         insertActionMenuItem(
           menu,
           position++,
           MainMenuAction::ToggleActiveImageTransformationLock,
           L"&Lock Transformation") &&
         insertActionMenuItem(
           menu,
           position++,
           MainMenuAction::ResetActiveImageManualTransformation,
           L"&Reset Manual Transformation to Identity") &&
         insertActionMenuItem(
           menu,
           position++,
           MainMenuAction::SaveActiveImageManualTransformation,
           L"Save &Manual Transformation...") &&
         insertActionMenuItem(
           menu,
           position++,
           MainMenuAction::SaveActiveImageInitialAndManualTransformation,
           L"Save &Initial + Manual Transformation...");
}

bool populateSegmentationMenu(HMENU menu)
{
  UINT position = 0;
  return insertActionMenuItem(
           menu,
           position++,
           MainMenuAction::ToggleSegmentationsWindow,
           L"Show Se&gmentation Window") &&
         insertSeparator(menu, position++) &&
         insertMenuItem(menu, position++, sk_addSegmentationCommand, L"&Add Segmentation...") &&
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
         insertActionMenuItem(menu, position++, MainMenuAction::IncreaseBrushSize, L"Increase Brush Size\t+") &&
         insertSeparator(menu, position++) &&
         insertActionMenuItem(
           menu,
           position++,
           MainMenuAction::PaintSegmentationFromAnnotation,
           L"&Paint Segmentation from Active Annotation");
}

bool populateAnnotationsMenu(HMENU menu)
{
  UINT position = 0;
  return insertActionMenuItem(menu, position++, MainMenuAction::ToggleAnnotationsWindow, L"Show &Annotations Window") &&
         insertSeparator(menu, position++) &&
         insertActionMenuItem(menu, position++, MainMenuAction::SaveAnnotations, L"&Save All Annotations...") &&
         insertActionMenuItem(menu, position++, MainMenuAction::RemoveAnnotation, L"&Remove Active Annotation") &&
         insertSeparator(menu, position++) &&
         insertActionMenuItem(menu, position++, MainMenuAction::MoveAnnotationBackward, L"Move &Backward") &&
         insertActionMenuItem(menu, position++, MainMenuAction::MoveAnnotationForward, L"Move &Forward") &&
         insertActionMenuItem(menu, position++, MainMenuAction::MoveAnnotationToBack, L"Move to Bac&k") &&
         insertActionMenuItem(menu, position++, MainMenuAction::MoveAnnotationToFront, L"Move to Fron&t") &&
         insertSeparator(menu, position++) &&
         insertActionMenuItem(
           menu,
           position++,
           MainMenuAction::PaintSegmentationFromAnnotation,
           L"&Paint Segmentation from Active Annotation");
}

bool populateLandmarksMenu(HMENU menu)
{
  UINT position = 0;
  return insertActionMenuItem(menu, position++, MainMenuAction::ToggleLandmarksWindow, L"Show &Landmarks Window") &&
         insertSeparator(menu, position++) &&
         insertActionMenuItem(menu, position++, MainMenuAction::CreateLandmarkGroup, L"&Create Landmark Group") &&
         insertActionMenuItem(menu, position++, MainMenuAction::SaveLandmarkGroup, L"&Save Active Landmark Group...") &&
         insertActionMenuItem(menu, position++, MainMenuAction::AddLandmark, L"&Add Landmark at Crosshairs") &&
         insertActionMenuItem(
           menu,
           position++,
           MainMenuAction::MoveCrosshairsToLandmark,
           L"&Move Crosshairs to Selected Landmark") &&
         insertActionMenuItem(menu, position++, MainMenuAction::RemoveLandmark, L"&Remove Selected Landmark");
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
  return insertActionMenuItem(menu, position++, MainMenuAction::Recenter, L"&Recenter\tC") &&
         insertActionMenuItem(menu, position++, MainMenuAction::ResetView, L"Reset Views and &Crosshairs\tShift+C") &&
         insertSeparator(menu, position++) &&
         insertActionMenuItem(menu, position++, MainMenuAction::ToggleImageVisibility, L"Show &Image\tW") &&
         insertActionMenuItem(
           menu,
           position++,
           MainMenuAction::ToggleSegmentationVisibility,
           L"Show &Segmentation\tS") &&
         insertActionMenuItem(menu, position++, MainMenuAction::ToggleImageEdges, L"Show Image &Edges\tE") &&
         insertActionMenuItem(
           menu,
           position++,
           MainMenuAction::ToggleSegmentationOutline,
           L"Toggle Segmentation &Outline\tSpace") &&
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
         insertActionMenuItem(menu, position++, MainMenuAction::ToggleScaleBars, L"Scale &Bars") &&
         insertActionMenuItem(menu, position++, MainMenuAction::ToggleOverlays, L"&Cycle Overlays\tO") &&
         insertActionMenuItem(menu, position++, MainMenuAction::ToggleFullScreen, L"&Full Screen\tF4") &&
         insertSeparator(menu, position++) && insertSubmenu(menu, position++, syncMenu, L"Synchronize with &ITK-SNAP");
}

bool populateWindowsMenu(HMENU menu)
{
  UINT position = 0;
  bool ok = insertActionMenuItem(menu, position++, MainMenuAction::ToggleImagesWindow, L"&Images") &&
            insertActionMenuItem(menu, position++, MainMenuAction::ToggleSegmentationsWindow, L"&Segmentations") &&
            insertActionMenuItem(menu, position++, MainMenuAction::ToggleAnnotationsWindow, L"&Annotations") &&
            insertActionMenuItem(menu, position++, MainMenuAction::ToggleLandmarksWindow, L"&Landmarks") &&
            insertActionMenuItem(menu, position++, MainMenuAction::ToggleIsosurfacesWindow, L"I&sosurfaces") &&
            insertActionMenuItem(menu, position++, MainMenuAction::ToggleSettingsWindow, L"Se&ttings") &&
            insertActionMenuItem(menu, position++, MainMenuAction::ToggleInspectorWindow, L"Ins&pector") &&
            insertActionMenuItem(menu, position++, MainMenuAction::ToggleOpacityMixerWindow, L"&Opacity Mixer") &&
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
  return insertMenuItem(menu, position++, sk_showAboutCommand, L"&About Entropy");
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

  if (
    !insertActionMenuItem(layoutsMenu, position++, MainMenuAction::AddLayout, L"&Add Layout") ||
    !insertActionMenuItem(layoutsMenu, position++, MainMenuAction::RemoveLayout, L"&Remove Current Layout") ||
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
    const UINT command = sk_selectActiveImageCommandBase + static_cast<UINT>(i);
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
  if (
    !state->mainMenu || !fileMenu || !modesMenu || !imageMenu || !segmentationMenu || !annotationsMenu ||
    !landmarksMenu || !state->layoutsMenu || !viewsMenu || !windowsMenu || !helpMenu || !state->activeImagesMenu)
  {
    if (state->mainMenu) {
      DestroyMenu(state->mainMenu);
    }
    return false;
  }

  if (
    !populateFileMenu(fileMenu) || !populateModesMenu(modesMenu) ||
    !populateActiveImagesMenu(state->activeImagesMenu, callbacks) ||
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
  RemoveWindowSubclass(hwnd, entropyMenuSubclassProc, sk_menuSubclassId);
  SetMenu(hwnd, nullptr);
  if (menu) {
    DestroyMenu(menu);
  }
  DrawMenuBar(hwnd);
  g_menuStates.erase(stateIt);
}
