#include "ui/headers/Headers.h"
#include "ui/GuiData.h"
#include "ui/headers/HeaderCommon.h"
#include "ui/DicomMetadataTable.h"
#include "ui/Helpers.h"
#include "ui/ImageExport.h"
#include "ui/ImGuiCustomControls.h"
#include "ui/NativeFileDialogs.h"
#include "ui/widgets/Widgets.h"
#include "ui/widgets/ImageHistogram.h"

// data::roundPointToNearestImageVoxelCenter
// data::getAnnotationSubjectPlaneName
#include "logic/app/DataHelper.h"

#include "image/DicomSeries.h"
#include "image/Image.h"
#include "image/ImageColorMap.h"
#include "image/ImageHeader.h"
#include "image/ImageSettings.h"
#include "image/ImageTransformations.h"
#include "image/ImageUtility.h"

#include "logic/app/Data.h"
#include "logic/states/annotation/AnnotationStateMachine.h"

#include <IconsForkAwesome.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/type_ptr.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/color_space.hpp>
#include <imgui-knobs.h>
#include <imgui/imgui.h>
#include <imgui/misc/cpp/imgui_stdlib.h>
#include <implot/implot.h>

#include <spdlog/fmt/ostr.h>
#include <spdlog/fmt/std.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#undef min
#undef max

namespace fs = std::filesystem;
using namespace entropy::ui::headers;

namespace
{
bool visibilityCheckboxBeforeSlider(const char* id, bool* visible, const char* tooltip)
{
  const float originalSliderWidth = ImGui::CalcItemWidth();
  const bool changed = ImGui::Checkbox(id, visible);
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("%s", tooltip);
  }
  const float checkboxSlotWidth = ImGui::GetItemRectSize().x + ImGui::GetStyle().ItemSpacing.x;
  ImGui::SameLine();
  ImGui::PushItemWidth(std::max(1.0f, originalSliderWidth - checkboxSlotWidth));
  return changed;
}
} // namespace

void renderSegmentationHeader(
  AppData& appData,
  const uuids::uuid& imageUid,
  size_t imageIndex,
  Image* image,
  bool isActiveImage,
  const std::function<void(void)>& updateImageUniforms,
  const std::function<ParcellationLabelTable*(size_t tableIndex)>& getLabelTable,
  const std::function<void(size_t tableIndex)>& updateLabelColorTableTexture,
  const std::function<void(size_t labelIndex)>& moveCrosshairsToSegLabelCentroid,
  const std::function<
    std::optional<uuids::uuid>(const uuids::uuid& matchingImageUid, const std::string& segDisplayName)>& createBlankSeg,
  const std::function<void(const uuids::uuid& imageUid, const fs::path& fileName)>& addSegmentationFile,
  const std::function<bool(const uuids::uuid& segUid)>& clearSeg,
  const std::function<bool(const uuids::uuid& segUid)>& removeSeg,
  const AllViewsRecenterType& recenterAllViews)
{
  static const std::string addSegFromFileString = std::string(ICON_FK_FOLDER_OPEN_O) + " Add...";
  static const std::string SaveSegString = std::string(ICON_FK_FLOPPY_O) + " Save...";
  static const std::string addNewSegString = std::string(ICON_FK_FILE_O) + " Create";
  static const std::string clearSegString = std::string(ICON_FK_ERASER) + " Clear";
  static const std::string removeSegString = std::string(ICON_FK_TRASH_O) + " Remove";

  if (!image) {
    spdlog::error("Null image");
    return;
  }

  const ImVec4* colors = ImGui::GetStyle().Colors;
  const ImVec4 activeColor = colors[ImGuiCol_ButtonActive];

  ImGuiTreeNodeFlags headerFlags = ImGuiTreeNodeFlags_CollapsingHeader;
  if (isActiveImage) {
    // Open header for the active image by default:
    headerFlags |= ImGuiTreeNodeFlags_DefaultOpen;
  }

  auto& imgSettings = image->settings();

  // Header is ID'ed only by the image index.
  // ### allows the header name to change without changing its ID.
  const bool isRef = appData.refImageUid() && *appData.refImageUid() == imageUid;
  const std::string headerName =
    std::to_string(imageIndex) + ") " +
    imageDisplayNameWithRole(imgSettings.displayName(), isRef, isActiveImage, appData.numImages()) + "###" +
    std::to_string(imageIndex);

  const auto headerColors = computeHeaderBgAndTextColors(imgSettings.borderColor());
  ImGui::PushStyleColor(ImGuiCol_Header, headerColors.first);
  ImGui::PushStyleColor(ImGuiCol_Text, headerColors.second);

  const bool open = ImGui::CollapsingHeader(headerName.c_str(), headerFlags);

  ImGui::PopStyleColor(2); // ImGuiCol_Header, ImGuiCol_Text

  if (!open) {
    return;
  }

  ImGui::Spacing();

  if (!isActiveImage) {
    if (ImGui::Button(ICON_FK_TOGGLE_OFF)) {
      if (appData.setActiveImageUid(imageUid)) {
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        return;
      }
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Make this the active image");
    }
  }
  else {
    ImGui::PushStyleColor(ImGuiCol_Button, activeColor);
    ImGui::Button(ICON_FK_TOGGLE_ON);
    ImGui::PopStyleColor(1); // ImGuiCol_Button
  }

  ImGui::SameLine();

  if (isRef && isActiveImage) {
    ImGui::Text("%s", referenceAndActiveImageMessage);
  }
  else if (isRef) {
    ImGui::Text("%s", referenceImageMessage);
  }
  else if (isActiveImage) {
    ImGui::Text("%s", activeImageMessage);
  }
  else {
    ImGui::Text("%s", nonActiveImageMessage);
  }

  const auto segUids = appData.imageToSegUids(imageUid);
  if (segUids.empty()) {
    ImGui::Text("This image has no segmentation");
    spdlog::error("Image {} has no segmentations", imageUid);
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    return;
  }

  auto activeSegUid = appData.imageToActiveSegUid(imageUid);
  if (!activeSegUid) {
    spdlog::error("Image {} has no active segmentation", imageUid);
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    return;
  }

  Image* activeSeg = appData.seg(*activeSegUid);
  if (!activeSeg) {
    spdlog::error("Active segmentation for image {} is null", imageUid);
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    return;
  }

  ImGui::PushID(uuids::to_string(*activeSegUid).c_str()); /*** PushID activeSegUid ***/

  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Spacing();

  //    ImGui::PushItemWidth(-1);
  if (ImGui::BeginCombo("Active seg.##Name", activeSeg->settings().displayName().c_str())) {
    size_t segIndex = 0;
    for (const auto& segUid : segUids) {
      ImGui::PushID(static_cast<int>(segIndex++));
      {
        if (Image* seg = appData.seg(segUid)) {
          const bool isSelected = (segUid == *activeSegUid);
          if (ImGui::Selectable(seg->settings().displayName().c_str(), isSelected)) {
            appData.assignActiveSegUidToImage(imageUid, segUid);
            activeSeg = appData.seg(segUid);
            updateImageUniforms();
          }

          if (isSelected) {
            ImGui::SetItemDefaultFocus();
          }
        }
      }
      ImGui::PopID(); // lmGroupIndex
    }
    ImGui::EndCombo();
  }
  //    ImGui::PopItemWidth();
  ImGui::SameLine();
  helpMarker("Select the active segmentation for this image");

  /// @todo Add button for copying the segmentation to a new segmentation

  // Add segmentation from file:
  ImGui::BeginDisabled(!addSegmentationFile);
  if (ImGui::Button(addSegFromFileString.c_str())) {
    if (const auto selectedFile = native_dialog::openFile(native_dialog::segmentationFilters())) {
      addSegmentationFile(imageUid, *selectedFile);
    }
  }
  ImGui::EndDisabled();
  if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
    ImGui::SetTooltip("Add a segmentation from file to this image");
  }

  // Save segmentation:
  static const char* dialogTitle("Select Segmentation Image");
  static const auto dialogFilters = native_dialog::segmentationFilters();

  ImGui::SameLine();
  const auto selectedFile = ImGui::renderFileButtonDialogAndWindow(SaveSegString.c_str(), dialogTitle, dialogFilters);

  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Save the segmentation to an image file on disk");
  }

  if (selectedFile) {
    static constexpr uint32_t compToSave = 0;
    if (activeSeg->saveComponentToDisk(compToSave, *selectedFile)) {
      spdlog::info("Saved segmentation image to file {}", *selectedFile);
      activeSeg->header().setFileName(*selectedFile);
    }
    else {
      spdlog::error("Error saving segmentation image to file {}", *selectedFile);
    }
  }

  // Create blank segmentation:
  ImGui::SameLine();
  if (ImGui::Button(addNewSegString.c_str())) {
    const size_t numSegsForImage = appData.imageToSegUids(imageUid).size();

    std::string segDisplayName = std::string("Untitled segmentation ") + std::to_string(numSegsForImage + 1) +
                                 " for image '" + image->settings().displayName() + "'";

    if (createBlankSeg(imageUid, std::move(segDisplayName))) {
      updateImageUniforms();
    }
    else {
      spdlog::error("Error creating new blank segmentation for image {}", imageUid);
    }
  }
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Create a new blank segmentation for this image");
  }

  // Clear segmentation:
  ImGui::SameLine();
  if (ImGui::Button(clearSegString.c_str())) {
    clearSeg(*activeSegUid);
  }
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Clear all values in this segmentation");
  }

  // Remove segmentation:
  // (Do not allow removal of the segmentation if it the only one for this image)
  if (appData.imageToSegUids(imageUid).size() > 1) {
    ImGui::SameLine();
    if (ImGui::Button(removeSegString.c_str())) {
      if (removeSeg(*activeSegUid)) {
        updateImageUniforms();
        ImGui::PopID(); /*** PopID activeSegUid ***/
        return;
      }
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Remove this segmentation from the image");
    }
  }

  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Spacing();

  // Double check that we still have the active segmentation:
  if (!activeSeg) {
    spdlog::error("Active segmentation for image {} is null", imageUid);
    ImGui::PopID(); /*** PopID activeSegUid ***/
    return;
  }

  auto& segSettings = activeSeg->settings();

  // Header is ID'ed only by "seg_" and the image index.
  // ### allows the header name to change without changing its ID.
  const std::string segHeaderName =
    std::string("Seg: ") + segSettings.displayName() + "###seg_" + std::to_string(imageIndex);

  /// @todo add "*" to end of name and change color of seg header if seg has been modified

  // Open segmentation View Properties on first appearance
  ImGui::SetNextItemOpen(true, ImGuiCond_Appearing);

  if (ImGui::TreeNode("View Properties")) {
    {
      bool segVisible = segSettings.visibility();
      if (visibilityCheckboxBeforeSlider(
            "##segmentationVisible",
            &segVisible,
            "Show/hide the segmentation on all views (S)"))
      {
        segSettings.setVisibility(segVisible);
        updateImageUniforms();
      }
      double segOpacity = segSettings.opacity();
      ImGui::BeginDisabled(!segVisible);
      if (mySliderF64("Opacity", &segOpacity, 0.0, 1.0)) {
        segSettings.setOpacity(segOpacity);
        updateImageUniforms();
      }
      ImGui::EndDisabled();
      ImGui::PopItemWidth();
      ImGui::SameLine();
      helpMarker("Segmentation layer opacity");
    }

    if (ImGui::BeginCombo("Sampling", typeString(segSettings.interpolationMode()).c_str())) {
      for (const auto& mode : {InterpolationMode::NearestNeighbor, InterpolationMode::Linear}) {
        if (ImGui::Selectable(typeString(mode).c_str(), (mode == segSettings.interpolationMode()))) {
          segSettings.setInterpolationMode(mode);
          if (mode == segSettings.interpolationMode()) {
            ImGui::SetItemDefaultFocus();
          }
        }
      }
      ImGui::EndCombo();
    }
    ImGui::SameLine();
    helpMarker("Select the segmentation interpolation type");

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::TreePop();
  }

  if (ImGui::TreeNode("Segmentation Labels")) {
    renderSegLabelsChildWindow(
      segSettings.labelTableIndex(),
      getLabelTable(segSettings.labelTableIndex()),
      updateLabelColorTableTexture,
      moveCrosshairsToSegLabelCentroid);

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::TreePop();
  }

  if (ImGui::TreeNode("Header")) {
    renderImageHeaderInformation(appData, imageUid, *activeSeg, updateImageUniforms, recenterAllViews);
    ImGui::TreePop();
  }

  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Spacing();
  ImGui::PopID(); /*** PopID activeSegUid ***/
}
