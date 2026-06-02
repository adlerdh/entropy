#include "ui/MainMenuBar.h"

#include <spdlog/fmt/std.h>
#include "ui/NativeFileDialogs.h"

#include <imgui/imgui.h>

namespace fs = std::filesystem;

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
        if (const auto selectedFile = native_dialog::openFile(native_dialog::imageFilters())) {
          if (callbacks.openImageFile) {
            callbacks.openImageFile(*selectedFile);
          }
        }
      }

      if (ImGui::MenuItem("Open Project...", nullptr, false, callbacks.canOpenProject)) {
        if (const auto selectedFile = native_dialog::openFile(native_dialog::projectFilters())) {
          if (callbacks.openProjectFile) {
            callbacks.openProjectFile(*selectedFile);
          }
        }
      }

      ImGui::Separator();

      if (ImGui::MenuItem("Add Image...", nullptr, false, callbacks.canAddImage)) {
        if (const auto selectedFile = native_dialog::openFile(native_dialog::imageFilters())) {
          if (callbacks.addImageFile) {
            callbacks.addImageFile(*selectedFile);
          }
        }
      }

      if (ImGui::MenuItem("Add Segmentation...", nullptr, false, callbacks.canAddSegmentation)) {
        if (const auto selectedFile = native_dialog::openFile(native_dialog::segmentationFilters())) {
          if (callbacks.addSegmentationFile) {
            callbacks.addSegmentationFile(*selectedFile);
          }
        }
      }

      ImGui::Separator();

      if (ImGui::MenuItem("Save Project", nullptr, false, callbacks.canSaveProject)) {
        const auto projectFileName = callbacks.projectFileName ? callbacks.projectFileName() : std::nullopt;

        if (projectFileName) {
          if (callbacks.saveProject) {
            callbacks.saveProject();
          }
        }
        else if (callbacks.saveProjectAs) {
          const fs::path defaultPath =
            callbacks.defaultProjectSaveDirectory ? callbacks.defaultProjectSaveDirectory() : fs::path{};
          const std::string defaultName =
            callbacks.defaultProjectSaveName ? callbacks.defaultProjectSaveName() : std::string{};
          if (
            const auto selectedFile =
              native_dialog::saveFile(native_dialog::projectFilters(), defaultPath, defaultName))
          {
            callbacks.saveProjectAs(*selectedFile);
          }
        }
      }

      if (ImGui::MenuItem("Save Project As...", nullptr, false, callbacks.canSaveProject)) {
        if (callbacks.saveProjectAs) {
          const fs::path defaultPath =
            callbacks.defaultProjectSaveDirectory ? callbacks.defaultProjectSaveDirectory() : fs::path{};
          const std::string defaultName =
            callbacks.defaultProjectSaveName ? callbacks.defaultProjectSaveName() : std::string{};
          if (
            const auto selectedFile =
              native_dialog::saveFile(native_dialog::projectFilters(), defaultPath, defaultName))
          {
            callbacks.saveProjectAs(*selectedFile);
          }
        }
      }

      ImGui::Separator();

      if (ImGui::MenuItem("Close Project", nullptr, false, callbacks.canCloseProject)) {
        if (callbacks.closeProject) {
          callbacks.closeProject();
        }
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
