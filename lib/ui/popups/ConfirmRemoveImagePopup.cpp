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
using namespace ui::popups;

void renderConfirmRemoveImagePopup(
  AppData& appData,
  const std::function<bool(const uuids::uuid& imageUid)>& removeImage)
{
  constexpr const char* popupTitle = "Remove Image?";

  const auto pendingUid = appData.guiData().m_pendingRemoveImageUid;
  const Image* pendingImage = pendingUid ? appData.image(*pendingUid) : nullptr;
  const std::string displayName = pendingImage ? pendingImage->settings().displayName() : std::string{"this image"};

  if (appData.guiData().m_showConfirmRemoveImagePopup) {
    const auto result = native_dialog::showMessageDialog(
      {popupTitle,
       "Remove '" + displayName + "' from the project?",
       "This removes the image from the current project and updates the saved project description.\n\n"
       "Segmentations, warp fields, landmarks, and annotations used only by this image will be removed from the "
       "project.\n\n"
       "Files on disk will not be deleted.\n\n"
       "Save the project after this operation if you want the removal preserved in the project file.",
       "Remove",
       "Cancel",
       ""});
    if (result) {
      if (native_dialog::MessageDialogResult::FirstButton == *result && pendingUid && removeImage) {
        removeImage(*pendingUid);
      }
      appData.guiData().m_pendingRemoveImageUid = std::nullopt;
      appData.guiData().m_showConfirmRemoveImagePopup = false;
      return;
    }
  }

  if (appData.guiData().m_showConfirmRemoveImagePopup && !ImGui::IsPopupOpen(popupTitle)) {
    ImGui::OpenPopup(popupTitle, ImGuiWindowFlags_Modal | ImGuiWindowFlags_AlwaysAutoResize);
  }

  const ImVec2 center(ImGui::GetIO().DisplaySize.x * 0.5f, ImGui::GetIO().DisplaySize.y * 0.5f);
  ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

  if (ImGui::BeginPopupModal(popupTitle, nullptr, ImGuiWindowFlags_Modal | ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::Text("Remove '%s' from the project?", displayName.c_str());
    ImGui::Spacing();
    ImGui::TextWrapped("This removes the image from the current project and updates the saved project description.");
    ImGui::Spacing();
    ImGui::BulletText(
      "Segmentations, warp fields, landmarks, and annotations used only by this image will be removed from the "
      "project.");
    ImGui::BulletText("Files on disk will not be deleted.");
    ImGui::BulletText("Save the project after this operation if you want the removal preserved in the project file.");
    ImGui::Separator();

    if (ImGui::Button("Yes")) {
      if (pendingUid && removeImage) {
        removeImage(*pendingUid);
      }
      appData.guiData().m_pendingRemoveImageUid = std::nullopt;
      ImGui::CloseCurrentPopup();
    }
    ImGui::SetItemDefaultFocus();

    ImGui::SameLine();
    if (ImGui::Button("No")) {
      appData.guiData().m_pendingRemoveImageUid = std::nullopt;
      ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
  }

  appData.guiData().m_showConfirmRemoveImagePopup = false;
}
