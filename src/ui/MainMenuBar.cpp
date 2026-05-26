#include "ui/MainMenuBar.h"

#include "ui/imgui/imgui-filebrowser/imfilebrowser.h"

#include <imgui/imgui.h>

namespace
{
ImGui::FileBrowser& imageOpenDialog()
{
  static ImGui::FileBrowser dialog(ImGuiFileBrowserFlags_CloseOnEsc);
  dialog.SetTitle("Open Image");
  dialog.SetTypeFilters({".nii", ".nii.gz", ".nrrd", ".nhdr", ".mha", ".mhd", ".dcm", ".img", ".hdr"});
  return dialog;
}

ImGui::FileBrowser& projectOpenDialog()
{
  static ImGui::FileBrowser dialog(ImGuiFileBrowserFlags_CloseOnEsc);
  dialog.SetTitle("Open Project");
  dialog.SetTypeFilters({".json"});
  return dialog;
}
} // namespace

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
        imageOpenDialog().Open();
      }

      if (ImGui::MenuItem("Open Project...", nullptr, false, callbacks.canOpenProject)) {
        projectOpenDialog().Open();
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

  auto& imageDialog = imageOpenDialog();
  imageDialog.Display();
  if (imageDialog.HasSelected()) {
    const fs::path selectedFile = imageDialog.GetSelected();
    imageDialog.ClearSelected();
    if (callbacks.openImageFile) {
      callbacks.openImageFile(selectedFile);
    }
  }

  auto& projectDialog = projectOpenDialog();
  projectDialog.Display();
  if (projectDialog.HasSelected()) {
    const fs::path selectedFile = projectDialog.GetSelected();
    projectDialog.ClearSelected();
    if (callbacks.openProjectFile) {
      callbacks.openProjectFile(selectedFile);
    }
  }
}
