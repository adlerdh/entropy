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

void renderConfirmSetReferenceImagePopup(
  AppData& appData,
  const std::function<bool(const uuids::uuid& imageUid)>& setReferenceImage)
{
  constexpr const char* popupTitle = "Set Reference Image?";

  if (appData.guiData().m_showConfirmSetReferenceImagePopup && !ImGui::IsPopupOpen(popupTitle)) {
    ImGui::OpenPopup(popupTitle, ImGuiWindowFlags_Modal | ImGuiWindowFlags_AlwaysAutoResize);
  }

  const ImVec2 center(ImGui::GetIO().DisplaySize.x * 0.5f, ImGui::GetIO().DisplaySize.y * 0.5f);
  ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

  if (ImGui::BeginPopupModal(popupTitle, nullptr, ImGuiWindowFlags_Modal | ImGuiWindowFlags_AlwaysAutoResize)) {
    const auto pendingUid = appData.guiData().m_pendingReferenceImageUid;
    const Image* pendingImage = pendingUid ? appData.image(*pendingUid) : nullptr;
    const std::string displayName = pendingImage ? pendingImage->settings().displayName() : std::string{"this image"};

    ImGui::Text("Make '%s' the reference image?", displayName.c_str());
    ImGui::Spacing();
    ImGui::TextWrapped("Changing the reference image changes the project coordinate frame.");
    ImGui::TextWrapped(
      "Entropy will update the other image transforms to preserve the current registration, but the selected image "
      "will become the fixed world-space reference.");
    ImGui::Spacing();
    ImGui::BulletText("The selected image's manual transform will be reset and locked.");
    ImGui::BulletText(
      "Any affine transform file assigned to the selected image will no longer be used as an affine transform for that "
      "image.");
    ImGui::BulletText(
      "Unsaved manual-registration changes should be saved after this operation if you want them in the project file.");
    ImGui::Separator();

    if (ImGui::Button("Yes")) {
      if (pendingUid && setReferenceImage) {
        setReferenceImage(*pendingUid);
      }
      appData.guiData().m_pendingReferenceImageUid = std::nullopt;
      ImGui::CloseCurrentPopup();
    }
    ImGui::SetItemDefaultFocus();

    ImGui::SameLine();
    if (ImGui::Button("No")) {
      appData.guiData().m_pendingReferenceImageUid = std::nullopt;
      ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
  }

  appData.guiData().m_showConfirmSetReferenceImagePopup = false;
}
