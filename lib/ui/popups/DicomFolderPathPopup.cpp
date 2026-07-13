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
using namespace ui::popups;

void renderDicomFolderPathPopup(
  AppData& appData,
  const std::function<void(const std::vector<std::filesystem::path>& folderNames)>& openDicomFolders)
{
  constexpr const char* popupTitle = "Open DICOM Folder";
  auto& guiData = appData.guiData();

  if (guiData.m_showDicomFolderPathPopup && !ImGui::IsPopupOpen(popupTitle)) {
    ImGui::OpenPopup(popupTitle, ImGuiWindowFlags_Modal | ImGuiWindowFlags_AlwaysAutoResize);
  }

  const ImVec2 center(ImGui::GetIO().DisplaySize.x * 0.5f, ImGui::GetIO().DisplaySize.y * 0.5f);
  ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
  ImGui::SetNextWindowSize(ImVec2(scaledPixel(760.0f), 0.0f), ImGuiCond_Appearing);
  setNextWindowSizeConstraintsToMainViewport();

  if (ImGui::BeginPopupModal(popupTitle, nullptr, ImGuiWindowFlags_Modal | ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::TextWrapped("Enter one or more DICOM folders. Put each folder on its own line.");
    ImGui::Spacing();
    ImGui::InputTextMultiline("##dicomFolders", &guiData.m_dicomFolderPathText, scaledSize(720.0f, 120.0f));

    auto setFolderText = [&guiData](const std::vector<std::filesystem::path>& folders) {
      if (folders.empty()) {
        return;
      }
      guiData.m_dicomFolderPathText.clear();
      for (const auto& folder : folders) {
        if (!guiData.m_dicomFolderPathText.empty()) {
          guiData.m_dicomFolderPathText += "\n";
        }
        guiData.m_dicomFolderPathText += folder.string();
      }
    };

    if (ImGui::Button("Browse Folders...")) {
      setFolderText(native_dialog::pickFolders());
    }

    ImGui::SameLine();
    if (ImGui::Button("Browse DICOM Files...")) {
      const auto selectedFiles = native_dialog::openFiles(native_dialog::imageFilters());
      std::vector<std::filesystem::path> folders;
      for (const auto& file : selectedFiles) {
        const auto parent = file.parent_path();
        if (!parent.empty() && std::find(folders.begin(), folders.end(), parent) == folders.end()) {
          folders.push_back(parent);
        }
      }
      setFolderText(folders);
    }

    ImGui::Separator();

    if (ImGui::Button("Scan")) {
      std::vector<std::filesystem::path> folderNames;
      std::istringstream stream(guiData.m_dicomFolderPathText);
      std::string line;
      while (std::getline(stream, line)) {
        line = trimWhitespace(line);
        if (!line.empty()) {
          folderNames.emplace_back(line);
        }
      }

      guiData.m_showDicomFolderPathPopup = false;
      ImGui::CloseCurrentPopup();
      if (!folderNames.empty() && openDicomFolders) {
        openDicomFolders(folderNames);
      }
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel")) {
      guiData.m_showDicomFolderPathPopup = false;
      ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
  }

  guiData.m_showDicomFolderPathPopup = false;
}
