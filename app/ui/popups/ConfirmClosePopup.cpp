#include "ui/popups/Popups.h"
#include "ui/popups/PopupCommon.h"
#include "ui/AboutIcon.h"
#include "ui/DicomMetadataTable.h"
#include "ui/Helpers.h"
#include "ui/NativeFileDialogs.h"
#include "ui/ThirdPartyLicenses.h"
#include "ui/dialogs/NativeMessageDialogs.h"

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

void renderConfirmCloseAppPopup(AppData& appData, const std::function<void(void)>& quitAppWithoutPrompt)
{
  constexpr const char* popupTitle = "Quit Entropy?";
  auto& guiData = appData.guiData();

  if (guiData.m_showConfirmCloseAppPopup) {
    const auto result =
      native_dialog::showMessageDialog({popupTitle, "Do you want to quit Entropy?", "", "Quit", "Cancel", ""});
    if (result) {
      guiData.m_showConfirmCloseAppPopup = false;
      if (native_dialog::MessageDialogResult::FirstButton == *result && quitAppWithoutPrompt) {
        quitAppWithoutPrompt();
      }
      return;
    }
  }

  if (guiData.m_showConfirmCloseAppPopup && !ImGui::IsPopupOpen(popupTitle)) {
    ImGui::OpenPopup(popupTitle, ImGuiWindowFlags_Modal | ImGuiWindowFlags_AlwaysAutoResize);
  }

  const ImVec2 center(ImGui::GetIO().DisplaySize.x * 0.5f, ImGui::GetIO().DisplaySize.y * 0.5f);
  ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

  if (ImGui::BeginPopupModal(popupTitle, nullptr, ImGuiWindowFlags_Modal | ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::Text("Do you want to quit Entropy?");
    ImGui::Separator();

    if (ImGui::Button("Quit")) {
      guiData.m_showConfirmCloseAppPopup = false;
      ImGui::CloseCurrentPopup();
      if (quitAppWithoutPrompt) {
        quitAppWithoutPrompt();
      }
    }
    ImGui::SetItemDefaultFocus();

    ImGui::SameLine();
    if (ImGui::Button("Cancel")) {
      guiData.m_showConfirmCloseAppPopup = false;
      ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
  }

  guiData.m_showConfirmCloseAppPopup = false;
}
