#include "ui/popups/Popups.h"
#include "ui/popups/PopupCommon.h"
#include "ui/AboutIcon.h"
#include "ui/DicomMetadataTable.h"
#include "ui/Helpers.h"
#include "ui/NativeFileDialogs.h"
#include "ui/ThirdPartyLicenses.h"

#include "image/Image.h"

#include "logic/app/AppPaths.h"
#include "logic/app/Data.h"

#include "BuildStamp.h"

#include <imgui/imgui.h>
#include <imgui/misc/cpp/imgui_stdlib.h>

#include <spdlog/fmt/std.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <initializer_list>
#include <limits>
#include <sstream>

namespace fs = std::filesystem;
using namespace entropy::ui::popups;

void renderUnsavedProjectPopup(
  AppData& appData,
  const std::function<bool(void)>& saveProject,
  const std::function<bool(const fs::path& fileName)>& saveProjectAs,
  const std::function<void(void)>& closeProjectWithoutPrompt,
  const std::function<void(void)>& quitAppWithoutPrompt,
  const std::function<fs::path(void)>& defaultProjectSaveDirectory,
  const std::function<std::string(void)>& defaultProjectSaveName)
{
  constexpr const char* popupTitle = "Unsaved Project";
  auto& guiData = appData.guiData();

  if (guiData.m_showUnsavedProjectPopup && !ImGui::IsPopupOpen(popupTitle)) {
    ImGui::OpenPopup(popupTitle, ImGuiWindowFlags_Modal | ImGuiWindowFlags_AlwaysAutoResize);
  }

  const ImVec2 center(ImGui::GetIO().DisplaySize.x * 0.5f, ImGui::GetIO().DisplaySize.y * 0.5f);
  ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

  if (ImGui::BeginPopupModal(popupTitle, nullptr, ImGuiWindowFlags_Modal | ImGuiWindowFlags_AlwaysAutoResize)) {
    const bool isQuit = GuiData::UnsavedProjectAction::QuitApp == guiData.m_pendingUnsavedProjectAction;
    ImGui::TextWrapped("The current project has unsaved changes.");
    ImGui::TextWrapped("Save before %s?", isQuit ? "quitting" : "closing it");
    ImGui::Separator();

    auto continueAction = [&]() {
      if (isQuit) {
        if (quitAppWithoutPrompt) {
          quitAppWithoutPrompt();
        }
      }
      else if (closeProjectWithoutPrompt) {
        closeProjectWithoutPrompt();
      }
    };

    if (ImGui::Button("Save")) {
      bool saved = false;
      if (appData.projectFileName()) {
        saved = saveProject ? saveProject() : false;
      }
      else if (saveProjectAs) {
        const fs::path defaultPath = defaultProjectSaveDirectory ? defaultProjectSaveDirectory() : fs::path{};
        const std::string defaultName = defaultProjectSaveName ? defaultProjectSaveName() : std::string{};
        if (
          const auto selectedFile = native_dialog::saveFile(native_dialog::projectFilters(), defaultPath, defaultName))
        {
          saved = saveProjectAs(*selectedFile);
        }
      }

      if (saved) {
        guiData.m_showUnsavedProjectPopup = false;
        ImGui::CloseCurrentPopup();
        continueAction();
      }
    }
    ImGui::SetItemDefaultFocus();

    ImGui::SameLine();
    if (ImGui::Button("Discard")) {
      guiData.m_showUnsavedProjectPopup = false;
      ImGui::CloseCurrentPopup();
      continueAction();
    }

    ImGui::SameLine();
    if (ImGui::Button("Cancel")) {
      guiData.m_showUnsavedProjectPopup = false;
      ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
  }

  guiData.m_showUnsavedProjectPopup = false;
}
