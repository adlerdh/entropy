#include "ui/MainMenuBar.h"

#include <spdlog/fmt/std.h>
#include "ui/GuiData.h"
#include "ui/NativeFileDialogs.h"

#include <imgui/imgui.h>

namespace fs = std::filesystem;

namespace main_menu
{
void openImage(const MainMenuBarCallbacks& callbacks)
{
  if (!callbacks.canOpenProject) {
    return;
  }

  if (const auto selectedFile = native_dialog::openFile(native_dialog::imageFilters())) {
    if (callbacks.openImageFile) {
      callbacks.openImageFile(*selectedFile);
    }
  }
}

void openProject(const MainMenuBarCallbacks& callbacks)
{
  if (!callbacks.canOpenProject) {
    return;
  }

  if (const auto selectedFile = native_dialog::openFile(native_dialog::projectFilters())) {
    if (callbacks.openProjectFile) {
      callbacks.openProjectFile(*selectedFile);
    }
  }
}

void addImage(const MainMenuBarCallbacks& callbacks)
{
  if (!callbacks.canAddImage) {
    return;
  }

  if (const auto selectedFile = native_dialog::openFile(native_dialog::imageFilters())) {
    if (callbacks.addImageFile) {
      callbacks.addImageFile(*selectedFile);
    }
  }
}

void addSegmentation(const MainMenuBarCallbacks& callbacks)
{
  if (!callbacks.canAddSegmentation) {
    return;
  }

  if (const auto selectedFile = native_dialog::openFile(native_dialog::segmentationFilters())) {
    if (callbacks.addSegmentationFile) {
      callbacks.addSegmentationFile(*selectedFile);
    }
  }
}

void saveProject(const MainMenuBarCallbacks& callbacks)
{
  if (!callbacks.canSaveProject) {
    return;
  }

  const auto projectFileName = callbacks.projectFileName ? callbacks.projectFileName() : std::nullopt;

  if (projectFileName) {
    if (callbacks.saveProject) {
      callbacks.saveProject();
    }
  }
  else {
    saveProjectAs(callbacks);
  }
}

void saveProjectAs(const MainMenuBarCallbacks& callbacks)
{
  if (!callbacks.canSaveProject || !callbacks.saveProjectAs) {
    return;
  }

  const fs::path defaultPath =
    callbacks.defaultProjectSaveDirectory ? callbacks.defaultProjectSaveDirectory() : fs::path{};
  const std::string defaultName = callbacks.defaultProjectSaveName ? callbacks.defaultProjectSaveName() : std::string{};
  if (const auto selectedFile = native_dialog::saveFile(native_dialog::projectFilters(), defaultPath, defaultName)) {
    callbacks.saveProjectAs(*selectedFile);
  }
}

void closeProject(const MainMenuBarCallbacks& callbacks)
{
  if (callbacks.canCloseProject && callbacks.closeProject) {
    callbacks.closeProject();
  }
}

void quitApp(const MainMenuBarCallbacks& callbacks)
{
  if (callbacks.quitApp) {
    callbacks.quitApp();
  }
}

void loadLayouts(const MainMenuBarCallbacks& callbacks)
{
  if (!callbacks.canUseLayouts || !callbacks.loadLayoutsFile) {
    return;
  }

  if (const auto selectedFile = native_dialog::openFile(native_dialog::layoutFilters())) {
    callbacks.loadLayoutsFile(*selectedFile);
  }
}

void saveLayouts(const MainMenuBarCallbacks& callbacks)
{
  if (!callbacks.canUseLayouts || !callbacks.saveLayoutsFile) {
    return;
  }

  const fs::path defaultPath =
    callbacks.defaultLayoutsSaveDirectory ? callbacks.defaultLayoutsSaveDirectory() : fs::path{};
  const std::string defaultName =
    callbacks.defaultLayoutsSaveName ? callbacks.defaultLayoutsSaveName() : "layouts.json";
  if (const auto selectedFile = native_dialog::saveFile(native_dialog::layoutFilters(), defaultPath, defaultName)) {
    callbacks.saveLayoutsFile(*selectedFile);
  }
}

bool actionEnabled(const MainMenuBarCallbacks& callbacks, MainMenuAction action)
{
  return callbacks.isActionEnabled ? callbacks.isActionEnabled(action) : false;
}

bool actionChecked(const MainMenuBarCallbacks& callbacks, MainMenuAction action)
{
  return callbacks.isActionChecked ? callbacks.isActionChecked(action) : false;
}

void performAction(const MainMenuBarCallbacks& callbacks, MainMenuAction action)
{
  if (actionEnabled(callbacks, action) && callbacks.performAction) {
    callbacks.performAction(action);
  }
}

bool actionMenuItem(
  const MainMenuBarCallbacks& callbacks,
  const char* label,
  MainMenuAction action,
  const char* shortcut = nullptr)
{
  if (ImGui::MenuItem(label, shortcut, actionChecked(callbacks, action), actionEnabled(callbacks, action))) {
    performAction(callbacks, action);
    return true;
  }
  return false;
}

void renderModeMenu(const MainMenuBarCallbacks& callbacks)
{
  actionMenuItem(callbacks, "Pointer", MainMenuAction::SetModePointer, "V");
  actionMenuItem(callbacks, "Window / Level", MainMenuAction::SetModeWindowLevel, "L");
  actionMenuItem(callbacks, "Zoom", MainMenuAction::SetModeZoom, "Z");
  actionMenuItem(callbacks, "Pan / Dolly", MainMenuAction::SetModePan, "X");
  actionMenuItem(callbacks, "Rotate View", MainMenuAction::SetModeRotateView);
  actionMenuItem(callbacks, "Rotate Crosshairs", MainMenuAction::SetModeRotateCrosshairs);
  ImGui::Separator();
  actionMenuItem(callbacks, "Brush", MainMenuAction::SetModeSegment, "B");
  actionMenuItem(callbacks, "Annotate", MainMenuAction::SetModeAnnotate);
  ImGui::Separator();
  actionMenuItem(callbacks, "Translate Image", MainMenuAction::SetModeTranslateImage, "T");
  actionMenuItem(callbacks, "Rotate Image", MainMenuAction::SetModeRotateImage, "R");
}

void renderViewsMenu(const MainMenuBarCallbacks& callbacks)
{
  actionMenuItem(callbacks, "Recenter", MainMenuAction::Recenter, "C");
  actionMenuItem(callbacks, "Reset Views and Crosshairs", MainMenuAction::ResetView, "Shift+C");
  ImGui::Separator();
  actionMenuItem(callbacks, "Show Image", MainMenuAction::ToggleImageVisibility, "W");
  actionMenuItem(callbacks, "Show Segmentation", MainMenuAction::ToggleSegmentationVisibility, "S");
  actionMenuItem(callbacks, "Show Image Edges", MainMenuAction::ToggleImageEdges, "E");
  actionMenuItem(callbacks, "Show Segmentation Outline", MainMenuAction::ToggleSegmentationOutline, "Space");
  actionMenuItem(callbacks, "Decrease Segmentation Opacity", MainMenuAction::DecreaseSegmentationOpacity, "A");
  actionMenuItem(callbacks, "Increase Segmentation Opacity", MainMenuAction::IncreaseSegmentationOpacity, "D");
  ImGui::Separator();
  actionMenuItem(callbacks, "Show Scale Bars", MainMenuAction::ToggleScaleBars);
  actionMenuItem(callbacks, "Cycle Overlays", MainMenuAction::ToggleOverlays, "O");
  actionMenuItem(callbacks, "Full Screen", MainMenuAction::ToggleFullScreen, "F4");
  ImGui::Separator();
  if (ImGui::BeginMenu("Synchronize with ITK-SNAP", actionEnabled(callbacks, MainMenuAction::ToggleSync))) {
    actionMenuItem(callbacks, "Enable Synchronization", MainMenuAction::ToggleSync);
    ImGui::Separator();
    actionMenuItem(callbacks, "Cursor: Send", MainMenuAction::ToggleSyncSendCursor);
    actionMenuItem(callbacks, "Cursor: Receive", MainMenuAction::ToggleSyncReceiveCursor);
    actionMenuItem(callbacks, "Zoom: Send", MainMenuAction::ToggleSyncSendZoom);
    actionMenuItem(callbacks, "Zoom: Receive", MainMenuAction::ToggleSyncReceiveZoom);
    actionMenuItem(callbacks, "Pan: Send", MainMenuAction::ToggleSyncSendPan);
    actionMenuItem(callbacks, "Pan: Receive", MainMenuAction::ToggleSyncReceivePan);
    ImGui::Separator();
    actionMenuItem(callbacks, "Settings...", MainMenuAction::ShowSynchronizeSettingsWindow);
    ImGui::EndMenu();
  }
}

void renderImageMenu(const MainMenuBarCallbacks& callbacks)
{
  actionMenuItem(callbacks, "Show Images Panel", MainMenuAction::ToggleImagesWindow);
  ImGui::Separator();
  if (ImGui::BeginMenu("Active Image", callbacks.canAddImage)) {
    const auto names = callbacks.imageNames ? callbacks.imageNames() : std::vector<std::string>{};
    const std::size_t activeIndex = callbacks.activeImageIndex ? callbacks.activeImageIndex() : 0;
    for (std::size_t i = 0; i < names.size(); ++i) {
      if (ImGui::MenuItem(names.at(i).c_str(), nullptr, i == activeIndex, callbacks.setActiveImageIndex != nullptr)) {
        callbacks.setActiveImageIndex(i);
      }
    }
    ImGui::EndMenu();
  }
  ImGui::Separator();
  actionMenuItem(callbacks, "Set as Reference", MainMenuAction::SetActiveImageAsReference);
  actionMenuItem(callbacks, "Remove Active Image", MainMenuAction::RemoveActiveImage);
  ImGui::Separator();
  actionMenuItem(callbacks, "Move Image Backward", MainMenuAction::MoveActiveImageBackward);
  actionMenuItem(callbacks, "Move Image Forward", MainMenuAction::MoveActiveImageForward);
  actionMenuItem(callbacks, "Move Image to Back", MainMenuAction::MoveActiveImageToBack);
  actionMenuItem(callbacks, "Move Image to Front", MainMenuAction::MoveActiveImageToFront);
  ImGui::Separator();
  actionMenuItem(callbacks, "Lock Transformation", MainMenuAction::ToggleActiveImageTransformationLock);
  actionMenuItem(callbacks, "Reset Manual Transformation", MainMenuAction::ResetActiveImageManualTransformation);
  actionMenuItem(callbacks, "Save Manual Transformation...", MainMenuAction::SaveActiveImageManualTransformation);
  actionMenuItem(
    callbacks,
    "Save Initial + Manual Transformation...",
    MainMenuAction::SaveActiveImageInitialAndManualTransformation);
}

void renderSegmentationMenu(const MainMenuBarCallbacks& callbacks)
{
  actionMenuItem(callbacks, "Show Segmentations Panel", MainMenuAction::ToggleSegmentationsWindow);
  ImGui::Separator();
  if (ImGui::MenuItem("Add Segmentation...", nullptr, false, callbacks.canAddSegmentation)) {
    addSegmentation(callbacks);
  }
  actionMenuItem(callbacks, "Create Blank Segmentation", MainMenuAction::CreateSegmentation);
  actionMenuItem(callbacks, "Save Active Segmentation...", MainMenuAction::SaveSegmentation);
  actionMenuItem(callbacks, "Clear Active Segmentation", MainMenuAction::ClearSegmentation);
  actionMenuItem(callbacks, "Remove Active Segmentation", MainMenuAction::RemoveSegmentation);
  ImGui::Separator();
  actionMenuItem(callbacks, "Previous Foreground Label", MainMenuAction::PreviousForegroundLabel, ",");
  actionMenuItem(callbacks, "Next Foreground Label", MainMenuAction::NextForegroundLabel, ".");
  actionMenuItem(callbacks, "Previous Background Label", MainMenuAction::PreviousBackgroundLabel, "<");
  actionMenuItem(callbacks, "Next Background Label", MainMenuAction::NextBackgroundLabel, ">");
  ImGui::Separator();
  actionMenuItem(callbacks, "Decrease Brush Size", MainMenuAction::DecreaseBrushSize, "-");
  actionMenuItem(callbacks, "Increase Brush Size", MainMenuAction::IncreaseBrushSize, "+");
}

void renderAnnotationMenu(const MainMenuBarCallbacks& callbacks)
{
  actionMenuItem(callbacks, "Show Annotations Panel", MainMenuAction::ToggleAnnotationsWindow);
  ImGui::Separator();
  actionMenuItem(callbacks, "Save All Annotations...", MainMenuAction::SaveAnnotations);
  actionMenuItem(callbacks, "Remove Active Annotation", MainMenuAction::RemoveAnnotation);
  ImGui::Separator();
  actionMenuItem(callbacks, "Move Backward", MainMenuAction::MoveAnnotationBackward);
  actionMenuItem(callbacks, "Move Forward", MainMenuAction::MoveAnnotationForward);
  actionMenuItem(callbacks, "Move to Back", MainMenuAction::MoveAnnotationToBack);
  actionMenuItem(callbacks, "Move to Front", MainMenuAction::MoveAnnotationToFront);
  ImGui::Separator();
  actionMenuItem(
    callbacks,
    "Paint Segmentation from Active Annotation",
    MainMenuAction::PaintSegmentationFromAnnotation);
}

void renderLandmarkMenu(const MainMenuBarCallbacks& callbacks)
{
  actionMenuItem(callbacks, "Show Landmarks Panel", MainMenuAction::ToggleLandmarksWindow);
  ImGui::Separator();
  actionMenuItem(callbacks, "Create Landmark Group", MainMenuAction::CreateLandmarkGroup);
  actionMenuItem(callbacks, "Save Active Landmark Group...", MainMenuAction::SaveLandmarkGroup);
  actionMenuItem(callbacks, "Add Landmark at Crosshairs", MainMenuAction::AddLandmark);
}

void handleFileShortcuts(const MainMenuBarCallbacks& callbacks)
{
  const ImGuiIO& io = ImGui::GetIO();
#ifdef __APPLE__
  const bool commandModifier = io.KeySuper;
#else
  const bool commandModifier = io.KeyCtrl;
#endif

  if (!commandModifier || io.KeyAlt || io.WantTextInput) {
    return;
  }

  if (ImGui::IsKeyPressed(ImGuiKey_O, false)) {
    if (io.KeyShift) {
      openProject(callbacks);
    }
    else {
      openImage(callbacks);
    }
  }
  else if (ImGui::IsKeyPressed(ImGuiKey_S, false)) {
    if (io.KeyShift) {
      saveProjectAs(callbacks);
    }
    else {
      saveProject(callbacks);
    }
  }
}
} // namespace main_menu

void renderMainMenuBar(GuiData& uiData, const MainMenuBarCallbacks& callbacks)
{
  main_menu::handleFileShortcuts(callbacks);

  if (!uiData.m_showMainMenuBar) {
    return;
  }

  if (ImGui::BeginMainMenuBar()) {
    const ImVec2 winSize = ImGui::GetWindowSize();
    uiData.m_mainMenuBarDims = glm::vec2{winSize.x, winSize.y};

    if (ImGui::BeginMenu("File")) {
      if (ImGui::MenuItem("Open Image...", "Ctrl+O", false, callbacks.canOpenProject)) {
        main_menu::openImage(callbacks);
      }

      if (ImGui::MenuItem("Open Project...", "Ctrl+Shift+O", false, callbacks.canOpenProject)) {
        main_menu::openProject(callbacks);
      }

      ImGui::Separator();

      if (ImGui::MenuItem("Add Image...", nullptr, false, callbacks.canAddImage)) {
        main_menu::addImage(callbacks);
      }

      if (ImGui::MenuItem("Add Segmentation...", nullptr, false, callbacks.canAddSegmentation)) {
        main_menu::addSegmentation(callbacks);
      }

      ImGui::Separator();

      if (ImGui::MenuItem("Save Project", "Ctrl+S", false, callbacks.canSaveProject)) {
        main_menu::saveProject(callbacks);
      }

      if (ImGui::MenuItem("Save Project As...", "Ctrl+Shift+S", false, callbacks.canSaveProject)) {
        main_menu::saveProjectAs(callbacks);
      }

      ImGui::Separator();

      if (ImGui::MenuItem("Close Project", nullptr, false, callbacks.canCloseProject)) {
        main_menu::closeProject(callbacks);
      }

      ImGui::Separator();

      if (ImGui::MenuItem("Quit", "Ctrl+Q", false, callbacks.quitApp != nullptr)) {
        main_menu::quitApp(callbacks);
      }

      ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Mode")) {
      main_menu::renderModeMenu(callbacks);
      ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Image", callbacks.canAddImage)) {
      main_menu::renderImageMenu(callbacks);
      ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Segmentation", callbacks.canAddSegmentation)) {
      main_menu::renderSegmentationMenu(callbacks);
      ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Annotation", callbacks.canUseLayouts)) {
      main_menu::renderAnnotationMenu(callbacks);
      ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Landmarks", callbacks.canUseLayouts)) {
      main_menu::renderLandmarkMenu(callbacks);
      ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Layout")) {
      if (ImGui::MenuItem("Load...", nullptr, false, callbacks.canUseLayouts)) {
        main_menu::loadLayouts(callbacks);
      }
      if (ImGui::MenuItem("Save...", nullptr, false, callbacks.canUseLayouts)) {
        main_menu::saveLayouts(callbacks);
      }

      ImGui::Separator();
      if (ImGui::MenuItem("Previous", "[", false, callbacks.canUseLayouts && callbacks.cycleLayouts)) {
        callbacks.cycleLayouts(-1);
      }
      if (ImGui::MenuItem("Next", "]", false, callbacks.canUseLayouts && callbacks.cycleLayouts)) {
        callbacks.cycleLayouts(1);
      }

      ImGui::Separator();
      if (ImGui::BeginMenu("Current Layout", callbacks.canUseLayouts)) {
        const auto names = callbacks.layoutNames ? callbacks.layoutNames() : std::vector<std::string>{};
        const std::size_t currentIndex = callbacks.currentLayoutIndex ? callbacks.currentLayoutIndex() : 0;
        for (std::size_t i = 0; i < names.size(); ++i) {
          const bool selected = i == currentIndex;
          if (ImGui::MenuItem(names.at(i).c_str(), nullptr, selected, callbacks.setCurrentLayoutIndex != nullptr)) {
            callbacks.setCurrentLayoutIndex(i);
          }
        }
        ImGui::EndMenu();
      }

      ImGui::Separator();
      main_menu::actionMenuItem(callbacks, "Add Layout", MainMenuAction::AddLayout);
      main_menu::actionMenuItem(callbacks, "Remove Current Layout", MainMenuAction::RemoveLayout);

      ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("View")) {
      main_menu::renderViewsMenu(callbacks);
      ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Window")) {
      static_cast<void>(uiData);
      main_menu::actionMenuItem(callbacks, "Images", MainMenuAction::ToggleImagesWindow);
      main_menu::actionMenuItem(callbacks, "Segmentations", MainMenuAction::ToggleSegmentationsWindow);
      main_menu::actionMenuItem(callbacks, "Annotations", MainMenuAction::ToggleAnnotationsWindow);
      main_menu::actionMenuItem(callbacks, "Landmarks", MainMenuAction::ToggleLandmarksWindow);
      main_menu::actionMenuItem(callbacks, "Isosurfaces", MainMenuAction::ToggleIsosurfacesWindow);
      main_menu::actionMenuItem(callbacks, "Settings", MainMenuAction::ToggleSettingsWindow);
      main_menu::actionMenuItem(callbacks, "Inspector", MainMenuAction::ToggleInspectorWindow);
      main_menu::actionMenuItem(callbacks, "Opacity Mixer", MainMenuAction::ToggleOpacityMixerWindow);
      ImGui::Separator();
      main_menu::actionMenuItem(callbacks, "Toolbar", MainMenuAction::ToggleToolbar);
#ifndef NDEBUG
      main_menu::actionMenuItem(callbacks, "ImGui Demo", MainMenuAction::ToggleImGuiDemoWindow);
      main_menu::actionMenuItem(callbacks, "ImPlot Demo", MainMenuAction::ToggleImPlotDemoWindow);
#endif
      ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Help")) {
      if (ImGui::MenuItem("About Entropy", nullptr, false, callbacks.showAbout != nullptr)) {
        callbacks.showAbout();
      }
      ImGui::EndMenu();
    }

    ImGui::EndMainMenuBar();
  }
}
