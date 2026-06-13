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

namespace
{

} // namespace

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
    ImGui::TextWrapped("Developers: %s; %s", PICSL_NAME, MIRAMONTE_NAME);
    ImGui::TextWrapped("Publisher: %s", PUBLISHER_NAME);

    ImGui::Spacing();
    ImGui::TextWrapped("%s", AUTHOR_CREDIT_LINE);
    ImGui::TextWrapped("%s", COPYRIGHT_LINE);

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
