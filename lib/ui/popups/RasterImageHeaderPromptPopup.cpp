#include "ui/popups/RasterImageHeaderPromptPopup.h"
#include "ui/dialogs/NativeMessageDialogs.h"
#include "ui/Helpers.h"
#include "ui/Scaling.h"

#include "logic/app/Data.h"

#include <glm/gtc/type_ptr.hpp>
#include <imgui/imgui.h>
#include <imgui/misc/cpp/imgui_stdlib.h>
#include <spdlog/fmt/fmt.h>

#include <filesystem>
#include <string>

namespace
{
void updateDerivedDirection(GuiData::RasterImageHeaderPrompt& prompt, ImageDirectionAxis editedAxis)
{
  std::string errorMessage;
  const auto directions =
    normalizedRasterDirectionMatrixAfterEdit(prompt.metadata.directions, editedAxis, &errorMessage);
  if (!directions) {
    prompt.validationMessage = errorMessage;
    return;
  }
  prompt.metadata.directions = *directions;
  prompt.validationMessage.clear();
}

std::string sizeLabel(std::uintmax_t bytes)
{
  const double mib = static_cast<double>(bytes) / (1024.0 * 1024.0);
  return std::to_string(bytes) + " bytes (" + fmt::format("{:.2f} MiB", mib) + ")";
}
} // namespace

void renderRasterImageHeaderPromptPopup(
  AppData& appData,
  const std::function<
    void(GuiData::RasterImageHeaderDecision decision, ImageSpatialMetadata metadata, bool applyToAll)>& handleDecision)
{
  constexpr const char* popupTitle = "Specify Image Geometry";
  GuiData& guiData = appData.guiData();

  if (guiData.m_showRasterImageHeaderPrompt && !ImGui::IsPopupOpen(popupTitle)) {
    ImGui::OpenPopup(popupTitle, ImGuiWindowFlags_Modal | ImGuiWindowFlags_AlwaysAutoResize);
    guiData.m_showRasterImageHeaderPrompt = false;
  }

  const ImVec2 center(ImGui::GetIO().DisplaySize.x * 0.5f, ImGui::GetIO().DisplaySize.y * 0.5f);
  ImGui::SetNextWindowPos(center, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
  const float popupWidth = ui::viewportClampedScaledSize(760.0f, 1.0f).x;
  ImGui::SetNextWindowSizeConstraints(ImVec2{popupWidth, 0.0f}, ImVec2{popupWidth, FLT_MAX});

  if (ImGui::BeginPopupModal(popupTitle, nullptr, ImGuiWindowFlags_Modal | ImGuiWindowFlags_AlwaysAutoResize)) {
    if (!guiData.m_pendingRasterImageHeaderPrompt) {
      ImGui::CloseCurrentPopup();
      ImGui::EndPopup();
      return;
    }

    auto& prompt = *guiData.m_pendingRasterImageHeaderPrompt;
    const glm::uvec3 dims = prompt.dimensions;
    const bool hasMultipleImages = prompt.allImages.size() > 1u;

    std::string path = prompt.fileName.string();
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Image path:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
    ImGui::InputText("##ImagePath", &path, ImGuiInputTextFlags_ReadOnly);
    ImGui::Text("Format: %s", prompt.format.c_str());
    ImGui::Text("Dimensions: %u x %u", dims.x, dims.y);
    ImGui::Text("Size: %s", sizeLabel(prompt.fileSizeBytes).c_str());
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::TextWrapped("Enter the physical image geometry to use for this image:");
    ImGui::InputFloat2("Spacing (mm/pixel)", glm::value_ptr(prompt.metadata.spacingMm), "%.6g");
    ImGui::SameLine();
    helpMarker("Physical pixel spacing in the image i and j directions, in millimeters per pixel");
    ImGui::InputFloat3("Origin (mm)", glm::value_ptr(prompt.metadata.originMm), "%.6g");
    ImGui::SameLine();
    helpMarker("Physical coordinates assigned to image pixel (0, 0)");

    ImGui::Spacing();

    auto directionInput = [&](const char* label, ImageDirectionAxis axis, const char* tooltip) {
      const int column = static_cast<int>(axis);
      const bool enterPressed = ImGui::InputFloat3(
        label,
        glm::value_ptr(prompt.metadata.directions[column]),
        "%.6g",
        ImGuiInputTextFlags_EnterReturnsTrue);
      if (enterPressed || ImGui::IsItemDeactivatedAfterEdit()) {
        updateDerivedDirection(prompt, axis);
      }
      ImGui::SameLine();
      helpMarker(tooltip);
    };
    directionInput(
      "i (column) direction",
      ImageDirectionAxis::I,
      "Unit direction vector for increasing image i coordinates, the column direction");
    directionInput(
      "j (row) direction",
      ImageDirectionAxis::J,
      "Unit direction vector for increasing image j coordinates, the row direction");
    directionInput(
      "k (slice) direction",
      ImageDirectionAxis::K,
      "Unit direction vector for increasing image k coordinates, the slice direction");

    if (hasMultipleImages) {
      ImGui::Separator();
      ImGui::Checkbox("Apply to all images being loaded:", &prompt.applyToAllImagesBeingLoaded);
      ImGui::SameLine();
      helpMarker("Use this same spacing, origin, and orientation for all standard 2D images in this load operation");
      ImGui::BeginChild("RasterImageHeaderImageList", ImVec2(ImGui::GetContentRegionAvail().x, 96.0f), true);
      for (const auto& fileName : prompt.allImages) {
        ImGui::TextWrapped("%s", fileName.string().c_str());
      }
      ImGui::EndChild();
    }

    std::string validationMessage;
    const bool valid = validateImageSpatialMetadata(prompt.metadata, &validationMessage);
    if (!valid || !prompt.validationMessage.empty()) {
      ImGui::TextColored(
        ImVec4(1.0f, 0.45f, 0.35f, 1.0f),
        "%s",
        prompt.validationMessage.empty() ? validationMessage.c_str() : prompt.validationMessage.c_str());
    }

    ImGui::Separator();
    if (!valid || !prompt.validationMessage.empty()) {
      ImGui::BeginDisabled();
    }
    if (ImGui::Button("OK")) {
      const ImageSpatialMetadata metadata = prompt.metadata;
      const bool applyToAll = prompt.applyToAllImagesBeingLoaded;
      guiData.m_pendingRasterImageHeaderPrompt = std::nullopt;
      guiData.m_showRasterImageHeaderPrompt = false;
      ImGui::CloseCurrentPopup();
      if (handleDecision) {
        handleDecision(GuiData::RasterImageHeaderDecision::Accept, metadata, applyToAll);
      }
    }
    if (!valid || !prompt.validationMessage.empty()) {
      ImGui::EndDisabled();
    }

    ImGui::SameLine();
    if (ImGui::Button("Cancel")) {
      const auto result = native_dialog::showMessageDialog(
        {"Cancel Image Load?",
         "Canceling will skip loading this image.",
         "Image path:\n" + prompt.fileName.string(),
         "Skip Image",
         "Keep Editing",
         ""});
      if (result && native_dialog::MessageDialogResult::FirstButton == *result) {
        const ImageSpatialMetadata metadata = prompt.metadata;
        guiData.m_pendingRasterImageHeaderPrompt = std::nullopt;
        guiData.m_showRasterImageHeaderPrompt = false;
        ImGui::CloseCurrentPopup();
        if (handleDecision) {
          handleDecision(GuiData::RasterImageHeaderDecision::Cancel, metadata, false);
        }
      }
    }

    ImGui::EndPopup();
  }
}
