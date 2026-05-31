#include "ui/Popups.h"
#include "ui/Helpers.h"
#include "ui/NativeFileDialogs.h"
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

void renderConfirmCloseAppPopup(AppData& appData, const std::function<void(void)>& quitAppWithoutPrompt)
{
  constexpr const char* popupTitle = "Quit Entropy?";
  auto& guiData = appData.guiData();

  if (guiData.m_showConfirmCloseAppPopup && !ImGui::IsPopupOpen(popupTitle)) {
    ImGui::OpenPopup(popupTitle, ImGuiWindowFlags_Modal | ImGuiWindowFlags_AlwaysAutoResize);
  }

  const ImVec2 center(ImGui::GetIO().DisplaySize.x * 0.5f, ImGui::GetIO().DisplaySize.y * 0.5f);
  ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

  if (ImGui::BeginPopupModal(popupTitle, nullptr, ImGuiWindowFlags_Modal | ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::Text("Do you want to quit Entropy?");
    ImGui::Separator();

    if (ImGui::Button("Quit", ImVec2(100, 0))) {
      guiData.m_showConfirmCloseAppPopup = false;
      ImGui::CloseCurrentPopup();
      if (quitAppWithoutPrompt) {
        quitAppWithoutPrompt();
      }
    }
    ImGui::SetItemDefaultFocus();

    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(100, 0))) {
      guiData.m_showConfirmCloseAppPopup = false;
      ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
  }

  guiData.m_showConfirmCloseAppPopup = false;
}

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

    if (ImGui::Button("Save", ImVec2(100, 0))) {
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
    if (ImGui::Button("Don't Save", ImVec2(100, 0))) {
      guiData.m_showUnsavedProjectPopup = false;
      ImGui::CloseCurrentPopup();
      continueAction();
    }

    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(100, 0))) {
      guiData.m_showUnsavedProjectPopup = false;
      ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
  }

  guiData.m_showUnsavedProjectPopup = false;
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

void renderLargeImageLoadPromptPopup(
  AppData& appData,
  const std::function<void(GuiData::LargeImageLoadDecision decision)>& handleDecision)
{
  constexpr const char* popupTitle = "Large Image Load";
  auto& guiData = appData.guiData();

  if (guiData.m_showLargeImageLoadPrompt && !ImGui::IsPopupOpen(popupTitle)) {
    ImGui::OpenPopup(popupTitle, ImGuiWindowFlags_Modal | ImGuiWindowFlags_AlwaysAutoResize);
  }

  const ImVec2 center(ImGui::GetIO().DisplaySize.x * 0.5f, ImGui::GetIO().DisplaySize.y * 0.5f);
  ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

  if (ImGui::BeginPopupModal(popupTitle, nullptr, ImGuiWindowFlags_Modal | ImGuiWindowFlags_AlwaysAutoResize)) {
    if (!guiData.m_pendingLargeImageLoadPrompt) {
      ImGui::CloseCurrentPopup();
      ImGui::EndPopup();
      return;
    }

    const auto& prompt = *guiData.m_pendingLargeImageLoadPrompt;
    const auto& header = prompt.header;
    const double fileGiB = static_cast<double>(header.fileImageSizeInBytes()) / (1024.0 * 1024.0 * 1024.0);
    const double memoryGiB = static_cast<double>(header.memoryImageSizeInBytes()) / (1024.0 * 1024.0 * 1024.0);
    const glm::uvec3 dims = header.pixelDimensions();

    ImGui::TextWrapped("This image is large and may take a while to load.");
    ImGui::Spacing();
    ImGui::Text("File: %s", prompt.fileName.string().c_str());
    ImGui::Text("Dimensions: %u x %u x %u", dims.x, dims.y, dims.z);
    ImGui::Text("Components: %u", header.numComponentsPerPixel());
    ImGui::Text("File type: %s", header.fileComponentTypeAsString().c_str());
    ImGui::Text("Memory type: %s", header.memoryComponentTypeAsString().c_str());
    ImGui::Text("Estimated file payload: %.2f GiB", fileGiB);
    ImGui::Text("Estimated memory payload: %.2f GiB", memoryGiB);
    ImGui::Separator();

    if (ImGui::Button("Load Original", ImVec2(130, 0))) {
      guiData.m_pendingLargeImageLoadPrompt = std::nullopt;
      guiData.m_showLargeImageLoadPrompt = false;
      ImGui::CloseCurrentPopup();
      if (handleDecision) {
        handleDecision(GuiData::LargeImageLoadDecision::LoadOriginal);
      }
    }
    ImGui::SetItemDefaultFocus();

    ImGui::SameLine();
    ImGui::BeginDisabled();
    ImGui::Button("Downsample", ImVec2(120, 0));
    ImGui::SameLine();
    ImGui::Button("Cast Smaller", ImVec2(120, 0));
    ImGui::EndDisabled();

    if (!prompt.allowSkipImage) {
      ImGui::BeginDisabled();
    }
    if (ImGui::Button("Skip Image", ImVec2(130, 0))) {
      guiData.m_pendingLargeImageLoadPrompt = std::nullopt;
      guiData.m_showLargeImageLoadPrompt = false;
      ImGui::CloseCurrentPopup();
      if (handleDecision) {
        handleDecision(GuiData::LargeImageLoadDecision::SkipImage);
      }
    }
    if (!prompt.allowSkipImage) {
      ImGui::EndDisabled();
    }

    if (prompt.allowCancelProject) {
      ImGui::SameLine();
      if (ImGui::Button("Cancel Project", ImVec2(130, 0))) {
        guiData.m_pendingLargeImageLoadPrompt = std::nullopt;
        guiData.m_showLargeImageLoadPrompt = false;
        ImGui::CloseCurrentPopup();
        if (handleDecision) {
          handleDecision(GuiData::LargeImageLoadDecision::CancelProject);
        }
      }
    }

    ImGui::Spacing();
    ImGui::TextDisabled("Downsample and cast options are planned but not enabled yet.");
    ImGui::EndPopup();
  }

  guiData.m_showLargeImageLoadPrompt = false;
}
