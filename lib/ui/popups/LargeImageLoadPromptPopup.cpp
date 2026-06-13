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
