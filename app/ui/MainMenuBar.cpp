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
} // namespace main_menu

void renderMainMenuBar(GuiData& uiData, const MainMenuBarCallbacks& callbacks)
{
  if (!uiData.m_showMainMenuBar) {
    return;
  }

  if (ImGui::BeginMainMenuBar()) {
    const ImVec2 winSize = ImGui::GetWindowSize();
    uiData.m_mainMenuBarDims = glm::vec2{winSize.x, winSize.y};

    if (ImGui::BeginMenu("File")) {
      if (ImGui::MenuItem("Open Image...", nullptr, false, callbacks.canOpenProject)) {
        main_menu::openImage(callbacks);
      }

      if (ImGui::MenuItem("Open Project...", nullptr, false, callbacks.canOpenProject)) {
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

      if (ImGui::MenuItem("Save Project", nullptr, false, callbacks.canSaveProject)) {
        main_menu::saveProject(callbacks);
      }

      if (ImGui::MenuItem("Save Project As...", nullptr, false, callbacks.canSaveProject)) {
        main_menu::saveProjectAs(callbacks);
      }

      ImGui::Separator();

      if (ImGui::MenuItem("Close Project", nullptr, false, callbacks.canCloseProject)) {
        main_menu::closeProject(callbacks);
      }

      ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Edit")) {
      if (ImGui::MenuItem("Item")) {
      }
      ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Modes")) {
      if (ImGui::MenuItem("Item")) {
      }
      ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Windows")) {
      if (ImGui::MenuItem("Item")) {
      }
      ImGui::EndMenu();
    }

    ImGui::EndMainMenuBar();
  }
}
