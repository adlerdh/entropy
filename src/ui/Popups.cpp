#include "ui/Popups.h"
#include "ui/Helpers.h"
#include "logic/app/Data.h"
#include "image/Image.h"

#include "BuildStamp.h"

#include <imgui/imgui.h>

void renderAddLayoutModalPopup(
  AppData& appData,
  bool openAddLayoutPopup,
  const std::function<void(void)>& recenterViews)
{
  bool addLayout = false;

  static int width = 3;
  static int height = 3;
  static bool isLightbox = false;

  if (openAddLayoutPopup && !ImGui::IsPopupOpen("Add Layout")) {
    ImGui::OpenPopup("Add Layout", ImGuiWindowFlags_AlwaysAutoResize);
  }

  const ImVec2 center(ImGui::GetIO().DisplaySize.x * 0.5f, ImGui::GetIO().DisplaySize.y * 0.5f);

  ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

  if (ImGui::BeginPopupModal("Add Layout", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::Text("Please set the number of views in the new layout:");

    if (ImGui::InputInt("Horizontal", &width)) {
      width = std::max(width, 1);
    }

    if (ImGui::InputInt("Vertical", &height)) {
      height = std::max(height, 1);
    }

    if (width >= 5 && height >= 5) {
      isLightbox = true;
    }

    ImGui::Checkbox("Lightbox mode", &isLightbox);
    ImGui::SameLine();
    helpMarker("Should all views in the layout share a common view type?");
    ImGui::Separator();

    ImGui::SetNextItemWidth(-1.0f);

    if (ImGui::Button("OK", ImVec2(80, 0))) {
      addLayout = true;
      ImGui::CloseCurrentPopup();
    }
    ImGui::SetItemDefaultFocus();

    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(80, 0))) {
      addLayout = false;
      ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
  }

  if (addLayout) {
    if (const auto& refUid = appData.refImageUid()) {
      // Apply offsets to views if using lightbox mode
      const bool offsetViews = (isLightbox);

      auto& wd = appData.windowData();

      wd.addGridLayout(
        ViewType::Axial,
        static_cast<std::size_t>(width),
        static_cast<std::size_t>(height),
        offsetViews,
        isLightbox,
        0,
        *refUid);

      wd.setCurrentLayoutIndex(wd.numLayouts() - 1);
      wd.setDefaultRenderedImagesForLayout(wd.currentLayout(), appData.imageUidsOrdered());

      recenterViews();
    }
  }
}

void renderAboutDialogModalPopup(bool open)
{
  static const std::string sk_gitInfo =
    std::string("Git:\n") + std::string("-branch: ") + GIT_BRANCH + "\n" + std::string("-commit: ") + GIT_COMMIT_SHA1 +
    "\n" + std::string("-timestamp: ") + GIT_COMMIT_TIMESTAMP + "\n\n" +

    std::string("Build:\n") + std::string("-timestamp: ") + BUILD_TIMESTAMP + " (UTC)\n" + std::string("-type: ") +
    CMAKE_BUILD_TYPE + " (shared libs: " + CMAKE_BUILD_SHARED_LIBS + ")\n" + std::string("-compiler: ") + COMPILER_ID +
    " (" + COMPILER_VERSION + ")\n" + std::string("-generator: ") + CMAKE_GENERATOR + "\n" + std::string("-CMake: ") +
    CMAKE_VERSION + "\n\n" +

    std::string("Host:\n") + std::string("-OS: ") + HOST_OS_NAME + " (" + HOST_OS_RELEASE + ", " + HOST_OS_VERSION +
    ")\n" + std::string("-system: ") + HOST_SYSTEM_NAME + " (" + HOST_SYSTEM_VERSION + ")\n" +
    std::string("-processor: ") + HOST_SYSTEM_PROCESSOR + " (" + HOST_PROCESSOR_NAME + ")\n" +
    std::string("-platform: ") + HOST_OS_PLATFORM;

  if (open && !ImGui::IsPopupOpen("About Entropy")) {
    ImGui::OpenPopup("About Entropy", ImGuiWindowFlags_AlwaysAutoResize);
  }

  const ImVec2 center(ImGui::GetIO().DisplaySize.x * 0.5f, ImGui::GetIO().DisplaySize.y * 0.5f);

  ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

  ImGui::SetNextWindowSize(ImVec2(600.0f, 0));

  if (ImGui::BeginPopupModal("About Entropy", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::Text("%s (version %s)", APP_NAME, VERSION_FULL);
    ImGui::Text("--> %s", APP_DESCRIPTION);

    ImGui::Spacing();
    ImGui::Text("%s,", ORG_NAME_1);
    ImGui::Text("%s", ORG_NAME_2);

    ImGui::Spacing();
    ImGui::Text("%s", COPYRIGHT_LINE);
    ImGui::Text("%s", LICENSE_LINE);

    ImGui::Spacing();
    ImGui::Spacing();
    ImGui::Text("Build information:");

    ImGui::InputTextMultiline(
      "##gitInfo",
      const_cast<char*>(sk_gitInfo.c_str()),
      sk_gitInfo.length(),
      ImVec2(-FLT_MIN, ImGui::GetTextLineHeight() * 18),
      ImGuiInputTextFlags_ReadOnly);

    if (ImGui::Button("Close", ImVec2(80, 0))) {
      ImGui::CloseCurrentPopup();
    }
    ImGui::SetItemDefaultFocus();

    ImGui::EndPopup();
  }
}

void renderConfirmCloseAppPopup(AppData& appData)
{
  if (appData.guiData().m_showConfirmCloseAppPopup && !ImGui::IsPopupOpen("Quit?")) {
    ImGui::OpenPopup("Quit?", ImGuiWindowFlags_Modal | ImGuiWindowFlags_NoDecoration);
  }

  const ImVec2 center(ImGui::GetIO().DisplaySize.x * 0.5f, ImGui::GetIO().DisplaySize.y * 0.5f);

  ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

  ImGui::SetNextItemWidth(-1.0f);
  if (ImGui::BeginPopupModal("Quit?", nullptr, ImGuiWindowFlags_Modal | ImGuiWindowFlags_NoDecoration)) {
    ImGui::Text("Do you want to quit?");
    ImGui::Separator();

    ImGui::SetNextItemWidth(-1.0f);

    if (ImGui::Button("Yes", ImVec2(80, 0))) {
      appData.state().setQuitApp(true);
      ImGui::CloseCurrentPopup();
    }
    ImGui::SetItemDefaultFocus();

    ImGui::SameLine();

    ImGui::SetNextItemWidth(-1.0f);

    if (ImGui::Button("No", ImVec2(80, 0))) {
      appData.state().setQuitApp(false);
      ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
  }

  // Disable the closing flag
  appData.guiData().m_showConfirmCloseAppPopup = false;
}

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

    if (ImGui::Button("Yes", ImVec2(80, 0))) {
      if (pendingUid && setReferenceImage) {
        setReferenceImage(*pendingUid);
      }
      appData.guiData().m_pendingReferenceImageUid = std::nullopt;
      ImGui::CloseCurrentPopup();
    }
    ImGui::SetItemDefaultFocus();

    ImGui::SameLine();
    if (ImGui::Button("No", ImVec2(80, 0))) {
      appData.guiData().m_pendingReferenceImageUid = std::nullopt;
      ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
  }

  appData.guiData().m_showConfirmSetReferenceImagePopup = false;
}

void renderConfirmRemoveImagePopup(
  AppData& appData,
  const std::function<bool(const uuids::uuid& imageUid)>& removeImage)
{
  constexpr const char* popupTitle = "Remove Image?";

  if (appData.guiData().m_showConfirmRemoveImagePopup && !ImGui::IsPopupOpen(popupTitle)) {
    ImGui::OpenPopup(popupTitle, ImGuiWindowFlags_Modal | ImGuiWindowFlags_AlwaysAutoResize);
  }

  const ImVec2 center(ImGui::GetIO().DisplaySize.x * 0.5f, ImGui::GetIO().DisplaySize.y * 0.5f);
  ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

  if (ImGui::BeginPopupModal(popupTitle, nullptr, ImGuiWindowFlags_Modal | ImGuiWindowFlags_AlwaysAutoResize)) {
    const auto pendingUid = appData.guiData().m_pendingRemoveImageUid;
    const Image* pendingImage = pendingUid ? appData.image(*pendingUid) : nullptr;
    const std::string displayName = pendingImage ? pendingImage->settings().displayName() : std::string{"this image"};

    ImGui::Text("Remove '%s' from the project?", displayName.c_str());
    ImGui::Spacing();
    ImGui::TextWrapped("This removes the image from the current project and updates the saved project description.");
    ImGui::Spacing();
    ImGui::BulletText(
      "Segmentations, deformation fields, landmarks, and annotations used only by this image will be removed from the "
      "project.");
    ImGui::BulletText("Files on disk will not be deleted.");
    ImGui::BulletText("Save the project after this operation if you want the removal preserved in the project file.");
    ImGui::Separator();

    if (ImGui::Button("Yes", ImVec2(80, 0))) {
      if (pendingUid && removeImage) {
        removeImage(*pendingUid);
      }
      appData.guiData().m_pendingRemoveImageUid = std::nullopt;
      ImGui::CloseCurrentPopup();
    }
    ImGui::SetItemDefaultFocus();

    ImGui::SameLine();
    if (ImGui::Button("No", ImVec2(80, 0))) {
      appData.guiData().m_pendingRemoveImageUid = std::nullopt;
      ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
  }

  appData.guiData().m_showConfirmRemoveImagePopup = false;
}
