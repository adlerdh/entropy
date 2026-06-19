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

void renderLandmarkGroupHeader(
  AppData& appData,
  const uuids::uuid& imageUid,
  size_t imageIndex,
  bool isActiveImage,
  const AllViewsRecenterType& recenterAllViews)
{
  static const char* newLmGroupButtonText("Create new group of landmarks");
  static const char* saveLmsButtonText("Save landmarks...");
  static const char* saveLmsDialogTitle("Save Landmark Group");
  static const auto saveLmsDialogFilters = native_dialog::landmarkFilters();

  Image* image = appData.image(imageUid);
  if (!image) {
    return;
  }

  auto addNewLmGroupButton = [&appData, &image, &imageUid]() {
    if (ImGui::Button(newLmGroupButtonText)) {
      LandmarkGroup newGroup;
      newGroup.setName(std::string("Landmarks for ") + image->settings().displayName());

      const auto newLmGroupUid = appData.addLandmarkGroup(std::move(newGroup));
      appData.assignLandmarkGroupUidToImage(imageUid, newLmGroupUid);
      appData.setRainbowColorsForAllLandmarkGroups();
      appData.assignActiveLandmarkGroupUidToImage(imageUid, newLmGroupUid);
    }
  };

  ImGuiTreeNodeFlags headerFlags = ImGuiTreeNodeFlags_CollapsingHeader;

  /// @todo This annoyingly pops up the active header each time... not sure why
  if (isActiveImage) {
    headerFlags |= ImGuiTreeNodeFlags_DefaultOpen;
  }

  ImGui::PushID(uuids::to_string(imageUid).c_str()); /** PushID imageUid **/

  // Header is ID'ed only by the image index.
  // ### allows the header name to change without changing its ID.
  const bool isRef = appData.refImageUid() && *appData.refImageUid() == imageUid;
  const std::string headerName =
    std::to_string(imageIndex) + ") " +
    imageDisplayNameWithRole(image->settings().displayName(), isRef, isActiveImage, appData.numImages()) + "###" +
    std::to_string(imageIndex);

  const auto imgSettings = image->settings();

  const auto headerColors = computeHeaderBgAndTextColors(imgSettings.borderColor());
  ImGui::PushStyleColor(ImGuiCol_Header, headerColors.first);
  ImGui::PushStyleColor(ImGuiCol_Text, headerColors.second);

  const bool open = ImGui::CollapsingHeader(headerName.c_str(), headerFlags);

  ImGui::PopStyleColor(2); // ImGuiCol_Header, ImGuiCol_Text

  if (!open) {
    ImGui::PopID(); // imageUid
    return;
  }

  ImGui::Spacing();

  const auto lmGroupUids = appData.imageToLandmarkGroupUids(imageUid);
  if (lmGroupUids.empty()) {
    ImGui::Text("This image has no landmarks.");
    addNewLmGroupButton();
    ImGui::PopID(); // imageUid
    return;
  }

  // Show a combo box if there are multiple landmark groups
  const bool showLmGroupCombo = (lmGroupUids.size() > 1);

  std::optional<uuids::uuid> activeLmGroupUid = appData.imageToActiveLandmarkGroupUid(imageUid);

  // The default active landmark group is at index 0
  if (!activeLmGroupUid) {
    if (appData.assignActiveLandmarkGroupUidToImage(imageUid, lmGroupUids[0])) {
      activeLmGroupUid = appData.imageToActiveLandmarkGroupUid(imageUid);
    }
    else {
      spdlog::error("Unable to assign active landmark group {} to image {}", lmGroupUids[0], imageUid);
      ImGui::PopID(); // imageUid
      return;
    }
  }

  LandmarkGroup* activeLmGroup = appData.landmarkGroup(*activeLmGroupUid);

  if (!activeLmGroup) {
    spdlog::error("Landmark group {} for image {} is null", *activeLmGroupUid, imageUid);
    ImGui::PopID(); // imageUid
    return;
  }

  if (showLmGroupCombo) {
    if (ImGui::BeginCombo("Landmark group", activeLmGroup->getName().c_str())) {
      size_t lmGroupIndex = 0;
      for (const auto& lmGroupUid : lmGroupUids) {
        ImGui::PushID(static_cast<int>(lmGroupIndex++));

        if (LandmarkGroup* lmGroup = appData.landmarkGroup(lmGroupUid)) {
          const bool isSelected = (lmGroupUid == *activeLmGroupUid);
          if (ImGui::Selectable(lmGroup->getName().c_str(), isSelected)) {
            appData.assignActiveLandmarkGroupUidToImage(imageUid, lmGroupUid);
            activeLmGroup = appData.landmarkGroup(lmGroupUid);
          }

          if (isSelected) {
            ImGui::SetItemDefaultFocus();
          }
        }
        ImGui::PopID(); // lmGroupIndex
      }

      ImGui::EndCombo();
    }

    ImGui::SameLine();
    helpMarker("Select the group of landmarks to view");

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
  }

  if (!activeLmGroup) {
    spdlog::error("Active landmark group for image {} is null", imageUid);
    ImGui::PopID(); // imageUid
    return;
  }

  // Landmark group display name:
  std::string groupName = activeLmGroup->getName();
  if (ImGui::InputText("Name", &groupName)) {
    activeLmGroup->setName(groupName);
  }
  ImGui::SameLine();
  helpMarker("Edit the name of the group of landmarks");

  // Landmark group file name:
  std::string fileName = activeLmGroup->getFileName().string();
  ImGui::InputText("File", &fileName, ImGuiInputTextFlags_ReadOnly);
  ImGui::SameLine();
  helpMarker("Comma-separated file with the landmarks");
  ImGui::Spacing();

  // Visibility checkbox:
  bool groupVisible = activeLmGroup->getVisibility();
  if (ImGui::Checkbox("Visible", &groupVisible)) {
    activeLmGroup->setVisibility(groupVisible);
  }
  ImGui::SameLine();
  helpMarker("Show/hide the landmarks");

  // Opacity slider:
  float groupOpacity = activeLmGroup->getOpacity();
  if (mySliderF32("Opacity", &groupOpacity, 0.0f, 1.0f)) {
    activeLmGroup->setOpacity(groupOpacity);
  }
  ImGui::SameLine();
  helpMarker("Landmark opacity");

  // Radius slider:
  float groupRadius = 100.0f * activeLmGroup->getRadiusFactor();
  if (mySliderF32("Radius", &groupRadius, 0.1f, 10.0f)) {
    activeLmGroup->setRadiusFactor(groupRadius / 100.0f);
  }
  ImGui::SameLine();
  helpMarker("Landmark circle radius");
  ImGui::Spacing();

  // Rendering of landmark indices:
  bool renderLandmarkIndices = activeLmGroup->getRenderLandmarkIndices();
  if (ImGui::Checkbox("Show indices", &renderLandmarkIndices)) {
    activeLmGroup->setRenderLandmarkIndices(renderLandmarkIndices);
  }
  ImGui::SameLine();
  helpMarker("Show/hide the landmark indices");

  // Rendering of landmark indices:
  bool renderLandmarkNames = activeLmGroup->getRenderLandmarkNames();
  if (ImGui::Checkbox("Show names", &renderLandmarkNames)) {
    activeLmGroup->setRenderLandmarkNames(renderLandmarkNames);
  }
  ImGui::SameLine();
  helpMarker("Show/hide the landmark names");

  // Uniform color for all landmarks:
  bool hasGroupColor = activeLmGroup->getColorOverride();

  if (ImGui::Checkbox("Global color", &hasGroupColor)) {
    activeLmGroup->setColorOverride(hasGroupColor);
  }

  if (hasGroupColor) {
    auto groupColor = activeLmGroup->getColor();
    ImGui::SameLine();
    if (ImGui::ColorEdit3("##uniformColor", glm::value_ptr(groupColor), colorEditFlags)) {
      activeLmGroup->setColor(groupColor);
    }
  }
  ImGui::SameLine();
  helpMarker("Set a global color for all landmarks in this group");

  // Text color for all landmarks:
  if (activeLmGroup->getTextColor().has_value()) {
    auto textColor = activeLmGroup->getTextColor().value();
    if (ImGui::ColorEdit3("Text color", glm::value_ptr(textColor), colorEditFlags)) {
      activeLmGroup->setTextColor(textColor);
    }
    ImGui::SameLine();
    helpMarker("Set text color for all landmarks");
    ImGui::Spacing();
  }

  // Voxel vs physical space radio buttons:
  ImGui::Spacing();
  ImGui::Text("Landmark coordinate space:");
  int inVoxelSpace = activeLmGroup->getInVoxelSpace() ? 1 : 0;

  if (ImGui::RadioButton("Physical subject (mm)", &inVoxelSpace, 0)) {
    activeLmGroup->setInVoxelSpace((1 == inVoxelSpace) ? true : false);
  }

  ImGui::SameLine();
  if (ImGui::RadioButton("Voxels", &inVoxelSpace, 1)) {
    activeLmGroup->setInVoxelSpace((1 == inVoxelSpace) ? true : false);
  }

  ImGui::SameLine();
  helpMarker("Space in which landmark coordinates are defined");
  ImGui::Spacing();

  // Child window for points:
  ImGui::Dummy(ImVec2(0.0f, 4.0f));

  auto setWorldCrosshairsPos = [&appData](const glm::vec3& worldCrosshairsPos) {
    appData.state().setWorldCrosshairsPos(worldCrosshairsPos);
  };

  renderLandmarkChildWindow(
    appData,
    image->transformations(),
    activeLmGroup,
    appData.state().worldCrosshairs().worldOrigin(),
    setWorldCrosshairsPos,
    recenterAllViews);

  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Spacing();

  addNewLmGroupButton();

  // Save landmarks to CSV and save settings to project file:
  const auto selectedFile =
    ImGui::renderFileButtonDialogAndWindow(saveLmsButtonText, saveLmsDialogTitle, saveLmsDialogFilters);

  ImGui::SameLine();
  helpMarker("Save the landmarks to a CSV file");

  if (selectedFile) {
    if (serialize::saveLandmarkGroupCsvFile(activeLmGroup->getPoints(), *selectedFile)) {
      spdlog::info("Saved landmarks to CSV file {}", *selectedFile);

      /// @todo How to handle changing the file name?
      activeLmGroup->setFileName(*selectedFile);
    }
    else {
      spdlog::error("Error saving landmarks to CSV file {}", *selectedFile);
    }
  }

  ImGui::Spacing();
  ImGui::PopID(); /** PopID imageUid **/
}
