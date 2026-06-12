#include "ui/Popups.h"
#include "ui/DicomMetadataTable.h"
#include "ui/AboutEntropyIcon.h"
#include "ui/Helpers.h"
#include "ui/NativeFileDialogs.h"
#include "ui/ThirdPartyLicenses.h"
#include <spdlog/fmt/std.h>
#include "logic/app/AppPaths.h"
#include "logic/app/Data.h"
#include "image/Image.h"

#include "BuildStamp.h"

#include <imgui/imgui.h>
#include <imgui/misc/cpp/imgui_stdlib.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <initializer_list>
#include <limits>
#include <sstream>

namespace fs = std::filesystem;

namespace
{

constexpr const char* sk_addLayoutPopupId = "Add Layout###AddLayoutModal";

float scaledPixel(float value)
{
  return value * (ImGui::GetFontSize() / 16.0f);
}

ImVec2 scaledSize(float x, float y)
{
  return ImVec2(scaledPixel(x), scaledPixel(y));
}

std::string displayPath(fs::path path)
{
  if (path.empty()) {
    return "<none>";
  }

  std::error_code error;
  if (path.is_relative()) {
    const fs::path absolutePath = fs::absolute(path, error);
    if (!error) {
      path = absolutePath;
    }
  }

  error.clear();
  const fs::path canonicalPath = fs::weakly_canonical(path, error);
  if (!error) {
    return canonicalPath.string();
  }

  return path.lexically_normal().string();
}

std::string currentDirectory()
{
  std::error_code error;
  const fs::path path = fs::current_path(error);
  return error ? std::string{"<unavailable>"} : displayPath(path);
}

void renderRuntimePathField(const char* label, const std::string& value, float inputWidth)
{
  ImGui::PushID(label);
  ImGui::SetNextItemWidth(inputWidth);
  ImGui::InputText("##value", const_cast<char*>(value.c_str()), value.size() + 1, ImGuiInputTextFlags_ReadOnly);
  ImGui::SameLine();
  ImGui::TextUnformatted(label);
  ImGui::PopID();
}

float runtimePathInputWidth(const std::initializer_list<const char*> labels)
{
  float labelWidth = 0.0f;
  for (const char* label : labels) {
    labelWidth = std::max(labelWidth, ImGui::CalcTextSize(label).x);
  }

  return std::max(
    ImGui::GetFontSize() * 12.0f,
    ImGui::GetContentRegionAvail().x - labelWidth - ImGui::GetStyle().ItemSpacing.x);
}

constexpr std::array<ViewType, 3> sk_lightboxViewTypes{ViewType::Axial, ViewType::Coronal, ViewType::Sagittal};

const char* lightboxViewTypeName(ViewType viewType)
{
  switch (viewType) {
    case ViewType::Axial:
      return "Axial";
    case ViewType::Coronal:
      return "Coronal";
    case ViewType::Sagittal:
      return "Sagittal";
    default:
      return "Axial";
  }
}

float defaultLightboxOffsetDistance(const AppData& appData, ViewType viewType)
{
  const Image* image = appData.refImage();
  if (!image) {
    image = appData.activeImage();
  }
  if (!image) {
    return 1.0f;
  }

  const glm::vec3& spacing = image->header().spacing();
  switch (viewType) {
    case ViewType::Sagittal:
      return spacing.x;
    case ViewType::Coronal:
      return spacing.y;
    case ViewType::Axial:
    default:
      return spacing.z;
  }
}

} // namespace

void renderAddLayoutModalPopup(
  AppData& appData,
  bool openAddLayoutPopup,
  const std::function<void(void)>& recenterViews)
{
  bool addLayout = false;

  static int width = 3;
  static int height = 3;
  static bool isLightbox = false;
  static ViewType lightboxViewType = ViewType::Axial;
  static float lightboxOffsetDistance = 1.0f;
  static ViewType lastLightboxViewType = ViewType::NumElements;

  if (openAddLayoutPopup && !ImGui::IsPopupOpen(sk_addLayoutPopupId)) {
    lightboxOffsetDistance = defaultLightboxOffsetDistance(appData, lightboxViewType);
    lastLightboxViewType = lightboxViewType;
    ImGui::OpenPopup(sk_addLayoutPopupId);
  }

  if (openAddLayoutPopup || ImGui::IsPopupOpen(sk_addLayoutPopupId)) {
    const ImVec2 center(ImGui::GetIO().DisplaySize.x * 0.5f, ImGui::GetIO().DisplaySize.y * 0.5f);
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
  }

  bool addLayoutDialogOpen = true;
  if (ImGui::BeginPopupModal(sk_addLayoutPopupId, &addLayoutDialogOpen, ImGuiWindowFlags_AlwaysAutoResize)) {
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

    if (isLightbox) {
      if (ImGui::BeginCombo("View type", lightboxViewTypeName(lightboxViewType))) {
        for (const ViewType candidate : sk_lightboxViewTypes) {
          const bool selected = candidate == lightboxViewType;
          if (ImGui::Selectable(lightboxViewTypeName(candidate), selected)) {
            lightboxViewType = candidate;
            lightboxOffsetDistance = defaultLightboxOffsetDistance(appData, lightboxViewType);
            lastLightboxViewType = lightboxViewType;
          }
          if (selected) {
            ImGui::SetItemDefaultFocus();
          }
        }
        ImGui::EndCombo();
      }
      ImGui::SameLine();
      helpMarker("Choose the anatomical plane used for every lightbox tile.");

      if (lastLightboxViewType != lightboxViewType) {
        lightboxOffsetDistance = defaultLightboxOffsetDistance(appData, lightboxViewType);
        lastLightboxViewType = lightboxViewType;
      }

      if (ImGui::InputFloat("Offset distance (mm)", &lightboxOffsetDistance, 0.1f, 1.0f, "%.3f")) {
        lightboxOffsetDistance = std::max(lightboxOffsetDistance, std::numeric_limits<float>::epsilon());
      }
      ImGui::SameLine();
      helpMarker("Distance between adjacent lightbox tiles along the selected view normal.");
    }

    ImGui::Separator();

    if (ImGui::Button("OK")) {
      addLayout = true;
      ImGui::CloseCurrentPopup();
    }
    ImGui::SetItemDefaultFocus();

    ImGui::SameLine();
    if (ImGui::Button("Cancel")) {
      addLayout = false;
      ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
  }

  if (addLayout) {
    if (const auto& refUid = appData.refImageUid()) {
      // Apply offsets to views if using lightbox mode
      const bool offsetViews = (isLightbox);
      const ViewType viewType = isLightbox ? lightboxViewType : ViewType::Axial;
      const std::optional<float> offsetDistance =
        isLightbox ? std::optional<float>{lightboxOffsetDistance} : std::nullopt;

      auto& wd = appData.windowData();

      wd.addGridLayout(
        viewType,
        static_cast<std::size_t>(width),
        static_cast<std::size_t>(height),
        offsetViews,
        isLightbox,
        0,
        *refUid,
        offsetDistance);

      wd.setCurrentLayoutIndex(wd.numLayouts() - 1);
      wd.setDefaultRenderedImagesForLayout(wd.currentLayout(), appData.imageUidsOrdered());

      recenterViews();
    }
  }
}

void renderAboutDialogModalPopup(bool open)
{
  static const std::string sk_buildInfo =
    std::string("Git version control:\n") + std::string("-branch: ") + GIT_BRANCH + "\n" + std::string("-commit: ") +
    GIT_COMMIT_SHA1 + "\n" + std::string("-timestamp: ") + GIT_COMMIT_TIMESTAMP + "\n\n" +

    std::string("Build:\n") + std::string("-timestamp: ") + BUILD_TIMESTAMP + " (UTC)\n" + std::string("-type: ") +
    CMAKE_BUILD_TYPE + " (shared libs: " + CMAKE_BUILD_SHARED_LIBS + ")\n" + std::string("-compiler: ") + COMPILER_ID +
    " (" + COMPILER_VERSION + ")\n" + std::string("-generator: ") + CMAKE_GENERATOR + "\n" + std::string("-CMake: ") +
    CMAKE_VERSION + "\n\n" +

    std::string("Host:\n") + std::string("-OS: ") + HOST_OS_NAME + " (" + HOST_OS_RELEASE + ", " + HOST_OS_VERSION +
    ")\n" + std::string("-system: ") + HOST_SYSTEM_NAME + " (" + HOST_SYSTEM_VERSION + ")\n" +
    std::string("-processor: ") + HOST_SYSTEM_PROCESSOR + " (" + HOST_PROCESSOR_NAME + ")\n" +
    std::string("-platform: ") + HOST_OS_PLATFORM + "\n\n";

  ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f), ImGuiCond_Always);
  ImGui::SetNextWindowSize(ImVec2(1.0f, 1.0f), ImGuiCond_Always);
  constexpr ImGuiWindowFlags hostFlags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs |
                                         ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
                                         ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoBackground;
  if (!ImGui::Begin("##AboutDialogPopupHost", nullptr, hostFlags)) {
    ImGui::End();
    return;
  }

  if (!open && !ImGui::IsPopupOpen("About Entropy")) {
    ImGui::End();
    return;
  }

  if (open && !ImGui::IsPopupOpen("About Entropy")) {
    ImGui::OpenPopup("About Entropy");
  }

  const ImVec2 center(ImGui::GetIO().DisplaySize.x * 0.5f, ImGui::GetIO().DisplaySize.y * 0.5f);

  ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

  ImGui::SetNextWindowSize(scaledSize(680.0f, 640.0f), ImGuiCond_Appearing);
  ImGui::SetNextWindowSizeConstraints(scaledSize(520.0f, 360.0f), ImVec2(FLT_MAX, FLT_MAX));

  bool aboutDialogOpen = true;
  if (ImGui::BeginPopupModal("About Entropy", &aboutDialogOpen)) {
    constexpr float iconSize = 72.0f;
    ImGui::Image(about_entropy_icon::textureId(), ImVec2(iconSize, iconSize));
    ImGui::SameLine();
    ImGui::BeginGroup();
    ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x);
    ImGui::Text("%s (version %s)", APP_NAME, VERSION_FULL);
    ImGui::TextWrapped("--> %s", APP_DESCRIPTION);
    ImGui::TextLinkOpenURL("https://github.com/adlerdh/entropy", "https://github.com/adlerdh/entropy");

    ImGui::Spacing();
    ImGui::TextWrapped("%s,", ORG_NAME_1);
    ImGui::TextWrapped("%s", ORG_NAME_2);

    ImGui::Spacing();
    ImGui::TextWrapped("Copyright 2021-2026 Penn Image Computing and Science Lab (PICSL),");
    ImGui::TextWrapped("University of Pennsylvania, and Daniel H. Adler.");
    ImGui::Spacing();
    ImGui::TextWrapped("%s", LICENSE_LINE);
    ImGui::PopTextWrapPos();
    ImGui::EndGroup();

    ImGui::Spacing();
    ImGui::Spacing();

    const ImGuiStyle& style = ImGui::GetStyle();
    const float closeButtonHeight = ImGui::GetFrameHeight();
    const float tabRegionHeight = std::max(
      ImGui::GetTextLineHeight() * 8.0f,
      ImGui::GetContentRegionAvail().y - closeButtonHeight - style.ItemSpacing.y);

    ImGui::BeginChild("##aboutTabRegion", ImVec2(-FLT_MIN, tabRegionHeight), false);
    if (ImGui::BeginTabBar("##aboutTabs")) {
      if (ImGui::BeginTabItem("Build information")) {
        ImGui::InputTextMultiline(
          "##buildInfo",
          const_cast<char*>(sk_buildInfo.c_str()),
          sk_buildInfo.length(),
          ImVec2(-FLT_MIN, -FLT_MIN),
          ImGuiInputTextFlags_ReadOnly);
        ImGui::EndTabItem();
      }

      if (ImGui::BeginTabItem("Runtime paths")) {
        const float inputWidth = runtimePathInputWidth(
          {"Current working directory",
           "Resource directory",
           "User data directory",
           "Log directory",
           "Uses platform user directories"});
        renderRuntimePathField("Current working directory", currentDirectory(), inputWidth);
        renderRuntimePathField("Resource directory", displayPath(app_paths::resourceDirectory()), inputWidth);
        renderRuntimePathField("User data directory", displayPath(app_paths::userDataDirectory()), inputWidth);
        renderRuntimePathField("Log directory", displayPath(app_paths::logDirectory()), inputWidth);
        renderRuntimePathField(
          "Uses platform user directories",
          app_paths::usesPlatformUserDirectories() ? "yes" : "no",
          inputWidth);
        ImGui::EndTabItem();
      }

      if (ImGui::BeginTabItem("External licenses")) {
        renderThirdPartyLicenses();
        ImGui::EndTabItem();
      }

      ImGui::EndTabBar();
    }
    ImGui::EndChild();

    if (ImGui::Button("Close")) {
      ImGui::CloseCurrentPopup();
    }
    ImGui::SetItemDefaultFocus();

    ImGui::EndPopup();
  }

  ImGui::End();
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

    if (ImGui::Button("Load Original")) {
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
    ImGui::Button("Downsample");
    ImGui::SameLine();
    ImGui::Button("Cast Smaller");
    ImGui::EndDisabled();

    if (!prompt.allowSkipImage) {
      ImGui::BeginDisabled();
    }
    if (ImGui::Button("Skip Image")) {
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
      if (ImGui::Button("Cancel Project")) {
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

namespace
{
std::string formatVec3(const glm::vec3& value)
{
  std::ostringstream ss;
  ss << value.x << ", " << value.y << ", " << value.z;
  return ss.str();
}

std::string formatUVec3(const glm::uvec3& value)
{
  return std::to_string(value.x) + " x " + std::to_string(value.y) + " x " + std::to_string(value.z);
}

std::string formatMat3(const glm::mat3& value)
{
  return formatVec3(value[0]) + " | " + formatVec3(value[1]) + " | " + formatVec3(value[2]);
}

glm::vec3 fieldOfViewMm(const dicom::SeriesGeometry& geometry)
{
  return glm::vec3{geometry.dimensions} * geometry.spacing;
}

std::string joinWarnings(const std::vector<std::string>& warnings)
{
  std::string result;
  for (const auto& warning : warnings) {
    if (!result.empty()) {
      result += "; ";
    }
    result += warning;
  }
  return result;
}

std::string trimWhitespace(const std::string& value)
{
  auto first = value.begin();
  while (first != value.end() && std::isspace(static_cast<unsigned char>(*first))) {
    ++first;
  }

  auto last = value.end();
  while (last != first && std::isspace(static_cast<unsigned char>(*(last - 1)))) {
    --last;
  }

  return std::string(first, last);
}

void renderSlicePreview(const dicom::SlicePreview& preview, float maxHeight, float maxWidth)
{
  if (preview.empty()) {
    ImGui::TextDisabled("No preview available.");
    return;
  }

  const float scale = std::max(
    0.25f,
    std::min(maxWidth / static_cast<float>(preview.width), maxHeight / static_cast<float>(preview.height)));
  const ImVec2 imageSize(static_cast<float>(preview.width) * scale, static_cast<float>(preview.height) * scale);
  const ImVec2 topLeft = ImGui::GetCursorScreenPos();
  ImDrawList* drawList = ImGui::GetWindowDrawList();

  for (std::uint32_t y = 0; y < preview.height; ++y) {
    for (std::uint32_t x = 0; x < preview.width; ++x) {
      const std::uint8_t value = preview.pixels.at(static_cast<std::size_t>(y) * preview.width + x);
      const ImU32 color = IM_COL32(value, value, value, 255);
      const ImVec2 p0(topLeft.x + static_cast<float>(x) * scale, topLeft.y + static_cast<float>(y) * scale);
      const ImVec2 p1(topLeft.x + static_cast<float>(x + 1) * scale, topLeft.y + static_cast<float>(y + 1) * scale);
      drawList->AddRectFilled(p0, p1, color);
    }
  }

  drawList->AddRect(topLeft, ImVec2(topLeft.x + imageSize.x, topLeft.y + imageSize.y), IM_COL32(120, 120, 120, 255));
  ImGui::InvisibleButton("##dicomPreviewImage", imageSize);
}

void renderDicomPreviewPanel(GuiData::DicomSeriesSelectionPrompt& prompt, float panelHeight)
{
  if (!prompt.previewSeriesIndex || *prompt.previewSeriesIndex >= prompt.series.size()) {
    ImGui::TextDisabled("Select a series row to preview its slices.");
    return;
  }

  const std::size_t index = *prompt.previewSeriesIndex;
  const auto& series = prompt.series.at(index);
  ImGui::Text("Preview: %s", series.displayName.c_str());
  ImGui::SameLine();
  ImGui::TextDisabled(
    "%s",
    series.geometry.sliceOrientation.empty() ? "Unknown orientation" : series.geometry.sliceOrientation.c_str());
  const int maxPreviewSlices = static_cast<int>(
    std::min<std::size_t>(series.files.size(), static_cast<std::size_t>(std::numeric_limits<int>::max())));
  bool previewCountChanged = false;
  if (maxPreviewSlices > 0 && prompt.previewSliceCount > maxPreviewSlices) {
    prompt.previewSliceCount = maxPreviewSlices;
    previewCountChanged = true;
  }
  if (prompt.previewSliceCount < 1) {
    prompt.previewSliceCount = 1;
    previewCountChanged = true;
  }

  ImGui::SameLine();
  ImGui::SetNextItemWidth(220.0f);
  previewCountChanged =
    ImGui::SliderInt("Slices", &prompt.previewSliceCount, 1, std::max(1, maxPreviewSlices)) || previewCountChanged;

  if (prompt.previewCache.size() != prompt.series.size()) {
    prompt.previewCache.resize(prompt.series.size());
  }
  if (prompt.previewErrors.size() != prompt.series.size()) {
    prompt.previewErrors.resize(prompt.series.size());
  }

  if (previewCountChanged && index < prompt.previewCache.size()) {
    prompt.previewCache.at(index).clear();
    prompt.previewErrors.at(index).clear();
  }

  if (prompt.previewCache.at(index).empty() && prompt.previewErrors.at(index).empty()) {
    prompt.previewCache.at(index) =
      dicom::loadSlicePreviews(series, 128, static_cast<std::size_t>(prompt.previewSliceCount));
    if (prompt.previewCache.at(index).empty()) {
      prompt.previewErrors.at(index) = "Unable to load preview.";
    }
  }

  if (!prompt.previewCache.at(index).empty()) {
    const ImGuiStyle& style = ImGui::GetStyle();
    const float headerHeight = ImGui::GetFrameHeightWithSpacing() + style.ItemSpacing.y;
    const float availableStripHeight = std::max(80.0f, panelHeight - headerHeight - style.ItemSpacing.y);
    const float labelHeight = ImGui::GetTextLineHeightWithSpacing();
    const float childPaddingY = 2.0f * style.WindowPadding.y;
    const float horizontalScrollbarHeight = style.ScrollbarSize;
    const float imageMaxHeight = std::max(
      48.0f,
      availableStripHeight - labelHeight - childPaddingY - horizontalScrollbarHeight - style.ItemSpacing.y);
    const float stripHeight =
      labelHeight + imageMaxHeight + childPaddingY + horizontalScrollbarHeight + style.ItemSpacing.y;
    ImGui::BeginChild("##dicomPreviewSlices", ImVec2(0.0f, stripHeight), false, ImGuiWindowFlags_HorizontalScrollbar);
    for (std::size_t i = 0; i < prompt.previewCache.at(index).size(); ++i) {
      if (i > 0) {
        ImGui::SameLine();
      }
      const auto& preview = prompt.previewCache.at(index).at(i);
      ImGui::PushID(static_cast<int>(i));
      ImGui::BeginGroup();
      ImGui::TextDisabled("%zu/%zu", preview.sliceIndex + 1, series.files.size());
      const float imageMaxWidth =
        imageMaxHeight * static_cast<float>(preview.width) / static_cast<float>(preview.height);
      renderSlicePreview(preview, imageMaxHeight, imageMaxWidth);
      ImGui::EndGroup();
      ImGui::PopID();
    }
    ImGui::EndChild();
  }
  else {
    ImGui::TextDisabled("%s", prompt.previewErrors.at(index).c_str());
  }
}
} // namespace

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

void renderDicomSeriesSelectionPopup(
  AppData& appData,
  const std::function<void(
    const std::vector<dicom::SeriesInfo>& series,
    std::optional<std::size_t> referenceSeriesIndex,
    bool addToExistingProject)>& loadSelectedSeries)
{
  constexpr const char* popupTitle = "Load DICOM Series";
  auto& guiData = appData.guiData();

  if (guiData.m_dicomSeriesScanInProgress) {
    ImGui::OpenPopup("Scanning DICOM Series", ImGuiWindowFlags_Modal | ImGuiWindowFlags_AlwaysAutoResize);
  }

  const ImVec2 center(ImGui::GetIO().DisplaySize.x * 0.5f, ImGui::GetIO().DisplaySize.y * 0.5f);
  ImGui::SetNextWindowSize(scaledSize(760.0f, 130.0f), ImGuiCond_Appearing);
  ImGui::SetNextWindowSizeConstraints(scaledSize(640.0f, 120.0f), ImVec2(FLT_MAX, FLT_MAX));
  ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
  if (ImGui::BeginPopupModal("Scanning DICOM Series", nullptr, ImGuiWindowFlags_Modal)) {
    if (!guiData.m_dicomSeriesScanInProgress) {
      ImGui::CloseCurrentPopup();
    }
    else {
      ImGui::Text("Scanning DICOM series...");
      if (!guiData.m_pendingDicomScanRoot.empty()) {
        ImGui::PushTextWrapPos(
          ImGui::GetCursorPosX() + std::max(scaledPixel(620.0f), ImGui::GetContentRegionAvail().x));
        ImGui::TextWrapped("%s", guiData.m_pendingDicomScanRoot.string().c_str());
        ImGui::PopTextWrapPos();
      }
    }
    ImGui::EndPopup();
  }

  if (guiData.m_showDicomSeriesSelectionPopup && !ImGui::IsPopupOpen(popupTitle)) {
    ImGui::OpenPopup(popupTitle, ImGuiWindowFlags_Modal);
  }

  ImGui::SetNextWindowSize(ImVec2(1480.0f, 680.0f), ImGuiCond_Appearing);
  ImGui::SetNextWindowSizeConstraints(ImVec2(980.0f, 520.0f), ImVec2(FLT_MAX, FLT_MAX));
  ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
  bool dicomSeriesDialogOpen = true;
  if (ImGui::BeginPopupModal(popupTitle, &dicomSeriesDialogOpen, ImGuiWindowFlags_Modal)) {
    if (!dicomSeriesDialogOpen) {
      guiData.m_pendingDicomSeriesSelectionPrompt = std::nullopt;
      guiData.m_showDicomSeriesSelectionPopup = false;
      ImGui::CloseCurrentPopup();
      ImGui::EndPopup();
      return;
    }
    if (!guiData.m_pendingDicomSeriesSelectionPrompt) {
      ImGui::CloseCurrentPopup();
      ImGui::EndPopup();
      return;
    }

    auto& prompt = *guiData.m_pendingDicomSeriesSelectionPrompt;
    const bool hasReference = appData.refImageUid().has_value();
    const bool allowReferenceColumn = prompt.allowReferenceSelection && !hasReference;

    ImGui::Text("Select DICOM series to load.");
    if (!prompt.allowReferenceSelection || hasReference) {
      ImGui::SameLine();
      ImGui::TextDisabled("Reference image is already set.");
    }

    for (const auto& warning : prompt.warnings) {
      ImGui::TextWrapped("Warning: %s", warning.c_str());
    }

    ImGui::Separator();

    constexpr std::array<const char*, 16> kSeriesColumnNames{
      "Load",
      "Reference",
      "Description",
      "Modality",
      "Series No.",
      "Slices",
      "Orientation",
      "Dimensions",
      "Spacing (mm)",
      "Field of View (mm)",
      "Origin",
      "Direction",
      "Series UID",
      "Study UID",
      "Metadata",
      "Warnings"};
    constexpr std::array<bool, 16> kSeriesColumnNoHide{
      true,
      true,
      false,
      false,
      false,
      false,
      false,
      false,
      false,
      false,
      false,
      false,
      false,
      false,
      true,
      false};

    if (ImGui::Button("Columns...")) {
      ImGui::OpenPopup("DICOM Series Columns");
    }
    if (ImGui::BeginPopup("DICOM Series Columns")) {
      for (std::size_t column = 0; column < prompt.seriesColumnVisible.size(); ++column) {
        if (column == 1 && !allowReferenceColumn) {
          continue;
        }
        if (kSeriesColumnNoHide.at(column)) {
          ImGui::BeginDisabled();
        }
        bool visible = prompt.seriesColumnVisible.at(column);
        if (ImGui::Checkbox(kSeriesColumnNames.at(column), &visible) && !kSeriesColumnNoHide.at(column)) {
          prompt.seriesColumnVisible.at(column) = visible;
        }
        if (kSeriesColumnNoHide.at(column)) {
          ImGui::EndDisabled();
        }
      }
      ImGui::EndPopup();
    }
    const float controlsHeight = 2.0f * ImGui::GetFrameHeightWithSpacing() + ImGui::GetStyle().ItemSpacing.y;
    const float previewHeight = std::max(230.0f, ImGui::GetContentRegionAvail().y * 0.36f);
    const float footerHeight = controlsHeight + previewHeight + 3.0f * ImGui::GetStyle().ItemSpacing.y;
    const float tableHeight = std::max(240.0f, ImGui::GetContentRegionAvail().y - footerHeight);

    if (ImGui::BeginTable(
          "DicomSeriesTableV3",
          16,
          ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_Reorderable |
            ImGuiTableFlags_Hideable | ImGuiTableFlags_ContextMenuInBody | ImGuiTableFlags_ScrollX |
            ImGuiTableFlags_ScrollY,
          ImVec2(0.0f, tableHeight)))
    {
      ImGui::TableSetupColumn("Load", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoHide, 52.0f);
      ImGui::TableSetupColumn("Reference", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoHide, 92.0f);
      ImGui::TableSetupColumn("Description", ImGuiTableColumnFlags_WidthFixed, 560.0f);
      ImGui::TableSetupColumn("Modality", ImGuiTableColumnFlags_WidthFixed, 84.0f);
      ImGui::TableSetupColumn("Series No.", ImGuiTableColumnFlags_WidthFixed, 82.0f);
      ImGui::TableSetupColumn("Slices", ImGuiTableColumnFlags_WidthFixed, 70.0f);
      ImGui::TableSetupColumn("Orientation", ImGuiTableColumnFlags_WidthFixed, 120.0f);
      ImGui::TableSetupColumn("Dimensions", ImGuiTableColumnFlags_WidthFixed, 128.0f);
      ImGui::TableSetupColumn("Spacing (mm)", ImGuiTableColumnFlags_WidthFixed, 172.0f);
      ImGui::TableSetupColumn("Field of View (mm)", ImGuiTableColumnFlags_WidthFixed, 172.0f);
      ImGui::TableSetupColumn("Origin", ImGuiTableColumnFlags_DefaultHide | ImGuiTableColumnFlags_WidthFixed, 172.0f);
      ImGui::TableSetupColumn(
        "Direction",
        ImGuiTableColumnFlags_DefaultHide | ImGuiTableColumnFlags_WidthFixed,
        320.0f);
      ImGui::TableSetupColumn("Series UID", ImGuiTableColumnFlags_WidthFixed, 560.0f);
      ImGui::TableSetupColumn("Study UID", ImGuiTableColumnFlags_WidthFixed, 520.0f);
      ImGui::TableSetupColumn("Metadata", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoHide, 92.0f);
      ImGui::TableSetupColumn("Warnings", ImGuiTableColumnFlags_WidthFixed, 320.0f);

      for (std::size_t column = 0; column < prompt.seriesColumnVisible.size(); ++column) {
        const bool columnVisible = prompt.seriesColumnVisible.at(column) && (column != 1 || allowReferenceColumn);
        ImGui::TableSetColumnEnabled(static_cast<int>(column), columnVisible);
      }

      ImGui::TableHeadersRow();

      for (std::size_t i = 0; i < prompt.series.size(); ++i) {
        const auto& series = prompt.series.at(i);
        const bool loadable = series.loadable();
        ImGui::PushID(static_cast<int>(i));
        ImGui::TableNextRow();

        ImGui::TableSetColumnIndex(0);
        if (!loadable) {
          ImGui::BeginDisabled();
        }
        bool selected = i < prompt.selected.size() && prompt.selected.at(i);
        if (ImGui::Checkbox("##load", &selected) && i < prompt.selected.size()) {
          prompt.selected.at(i) = selected;
          prompt.previewSeriesIndex = i;
          if (selected && prompt.referenceSeriesIndex < 0) {
            prompt.referenceSeriesIndex = static_cast<int>(i);
          }
        }
        if (!loadable) {
          ImGui::EndDisabled();
        }

        ImGui::TableSetColumnIndex(1);
        const bool referenceEnabled = prompt.allowReferenceSelection && selected && loadable;
        if (!referenceEnabled) {
          ImGui::BeginDisabled();
        }
        if (ImGui::RadioButton("##ref", &prompt.referenceSeriesIndex, static_cast<int>(i))) {
          prompt.previewSeriesIndex = i;
        }
        if (!referenceEnabled) {
          ImGui::EndDisabled();
        }

        ImGui::TableSetColumnIndex(2);
        if (ImGui::Selectable(
              series.displayName.c_str(),
              prompt.previewSeriesIndex && *prompt.previewSeriesIndex == i,
              ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap))
        {
          prompt.previewSeriesIndex = i;
        }
        ImGui::TableSetColumnIndex(3);
        ImGui::TextUnformatted(series.metadata.modality.c_str());
        ImGui::TableSetColumnIndex(4);
        ImGui::TextUnformatted(series.metadata.seriesNumber.c_str());
        ImGui::TableSetColumnIndex(5);
        ImGui::Text("%zu", series.files.size());
        ImGui::TableSetColumnIndex(6);
        ImGui::TextUnformatted(series.geometry.sliceOrientation.c_str());
        ImGui::TableSetColumnIndex(7);
        const std::string dims = formatUVec3(series.geometry.dimensions);
        ImGui::TextUnformatted(dims.c_str());
        ImGui::TableSetColumnIndex(8);
        const std::string spacing = formatVec3(series.geometry.spacing);
        ImGui::TextUnformatted(spacing.c_str());
        ImGui::TableSetColumnIndex(9);
        const std::string fov = formatVec3(fieldOfViewMm(series.geometry));
        ImGui::TextUnformatted(fov.c_str());
        ImGui::TableSetColumnIndex(10);
        const std::string origin = formatVec3(series.geometry.origin);
        ImGui::TextUnformatted(origin.c_str());
        ImGui::TableSetColumnIndex(11);
        const std::string direction = formatMat3(series.geometry.directions);
        ImGui::TextWrapped("%s", direction.c_str());
        ImGui::TableSetColumnIndex(12);
        ImGui::TextUnformatted(series.seriesInstanceUid.c_str());
        ImGui::TableSetColumnIndex(13);
        ImGui::TextUnformatted(series.metadata.studyInstanceUid.c_str());
        ImGui::TableSetColumnIndex(14);
        if (ImGui::SmallButton("View...")) {
          prompt.metadataSeriesIndex = i;
        }
        ImGui::TableSetColumnIndex(15);
        const std::string warnings = joinWarnings(series.warnings);
        if (!warnings.empty()) {
          ImGui::TextWrapped("%s", warnings.c_str());
        }
        ImGui::PopID();
      }
      ImGui::EndTable();
    }

    ImGui::BeginChild("##dicomPreviewPanel", ImVec2(0.0f, previewHeight), true);
    renderDicomPreviewPanel(prompt, previewHeight);
    ImGui::EndChild();

    if (prompt.metadataSeriesIndex && *prompt.metadataSeriesIndex < prompt.series.size()) {
      ImGui::OpenPopup("DICOM Metadata");
    }

    ImGui::SetNextWindowSize(ImVec2(1100.0f, 620.0f), ImGuiCond_Appearing);
    ImGui::SetNextWindowSizeConstraints(ImVec2(760.0f, 420.0f), ImVec2(FLT_MAX, FLT_MAX));
    bool dicomMetadataDialogOpen = true;
    if (ImGui::BeginPopupModal("DICOM Metadata", &dicomMetadataDialogOpen, ImGuiWindowFlags_Modal)) {
      if (!dicomMetadataDialogOpen) {
        prompt.metadataSeriesIndex = std::nullopt;
        ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
        return;
      }
      if (!prompt.metadataSeriesIndex || *prompt.metadataSeriesIndex >= prompt.series.size()) {
        ImGui::CloseCurrentPopup();
      }
      else {
        const auto& series = prompt.series.at(*prompt.metadataSeriesIndex);
        ImGui::TextWrapped("%s", series.displayName.c_str());
        ImGui::TextWrapped("Study UID: %s", series.metadata.studyInstanceUid.c_str());
        ImGui::TextWrapped("Series UID: %s", series.seriesInstanceUid.c_str());
        ImGui::Text("Slices: %zu", series.files.size());
        ImGui::Separator();

        const float metadataFooterHeight = ImGui::GetFrameHeightWithSpacing() + ImGui::GetStyle().ItemSpacing.y;
        const ImVec2 metadataTableSize(
          ImGui::GetContentRegionAvail().x,
          std::max(220.0f, ImGui::GetContentRegionAvail().y - metadataFooterHeight));

        renderDicomMetadataTable("DicomMetadataTable", series.metadataSummary, metadataTableSize);

        if (ImGui::Button("Close")) {
          prompt.metadataSeriesIndex = std::nullopt;
          ImGui::CloseCurrentPopup();
        }
      }
      ImGui::EndPopup();
    }

    const float footerPadding = ImGui::GetContentRegionAvail().y - controlsHeight;
    if (footerPadding > 0.0f) {
      ImGui::Dummy(ImVec2(0.0f, footerPadding));
    }

    std::size_t selectedCount = 0;
    for (bool selected : prompt.selected) {
      if (selected) {
        ++selectedCount;
      }
    }

    if (ImGui::Button("Select All Loadable")) {
      for (std::size_t i = 0; i < prompt.series.size() && i < prompt.selected.size(); ++i) {
        prompt.selected.at(i) = prompt.series.at(i).loadable();
      }
    }
    ImGui::SameLine();
    if (ImGui::Button("Clear Selection")) {
      std::fill(prompt.selected.begin(), prompt.selected.end(), false);
    }

    ImGui::SameLine();
    ImGui::TextDisabled("%zu selected", selectedCount);
    ImGui::Separator();

    const bool canLoad = selectedCount > 0;
    if (!canLoad) {
      ImGui::BeginDisabled();
    }
    if (ImGui::Button("Load")) {
      std::vector<dicom::SeriesInfo> selectedSeries;
      selectedSeries.reserve(selectedCount);
      std::optional<std::size_t> referenceIndex;
      for (std::size_t i = 0; i < prompt.series.size() && i < prompt.selected.size(); ++i) {
        if (!prompt.selected.at(i)) {
          continue;
        }
        if (prompt.allowReferenceSelection && prompt.referenceSeriesIndex == static_cast<int>(i)) {
          referenceIndex = selectedSeries.size();
        }
        selectedSeries.push_back(prompt.series.at(i));
      }
      const bool addToExistingProject = prompt.addToExistingProject;
      guiData.m_pendingDicomSeriesSelectionPrompt = std::nullopt;
      guiData.m_showDicomSeriesSelectionPopup = false;
      ImGui::CloseCurrentPopup();
      if (loadSelectedSeries) {
        loadSelectedSeries(selectedSeries, referenceIndex, addToExistingProject);
      }
    }
    if (!canLoad) {
      ImGui::EndDisabled();
    }

    ImGui::SameLine();
    if (ImGui::Button("Cancel")) {
      guiData.m_pendingDicomSeriesSelectionPrompt = std::nullopt;
      guiData.m_showDicomSeriesSelectionPopup = false;
      ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
  }

  guiData.m_showDicomSeriesSelectionPopup = false;
}
