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
using namespace ui::headers;

void renderAnnotationsHeader(
  AppData& appData,
  const uuids::uuid& imageUid,
  size_t imageIndex,
  bool isActiveImage,
  bool hasFollowingHeader,
  const std::function<void(const uuids::uuid& viewUid, const glm::vec3& worldFwdDirection)>& setViewDirection,
  const std::function<void()>& paintActiveSegmentationWithActivePolygon,
  const AllViewsRecenterType& recenterAllViews)
{
  static constexpr bool doNotRecenterCrosshairs = false;
  static constexpr bool doNotRealignCrosshairs = false;
  static constexpr bool doNotRecenterOnCurrentCrosshairsPosition = false;
  static constexpr bool doNotResetObliqueOrientation = false;
  static constexpr bool doNotResetZoom = false;

  static constexpr size_t minNumLines = 6;
  static constexpr size_t maxNumLines = 12;

  static const ImGuiColorEditFlags annotColorEditFlags =
    ImGuiColorEditFlags_PickerHueBar | ImGuiColorEditFlags_DisplayRGB | ImGuiColorEditFlags_DisplayHex |
    ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_AlphaPreviewHalf | ImGuiColorEditFlags_Uint8 |
    ImGuiColorEditFlags_InputRGB;

  static const std::string saveAnnotButtonText = std::string(ICON_FK_FLOPPY_O) + " Save all...";
  static const std::string removeAnnotButtonText = std::string(ICON_FK_TRASH_O) + " Remove";
  static const std::string fillAnnotButtonText = std::string(ICON_FK_PAINT_BRUSH) + " Fill segmentation";

  static const char* saveAnnotDialogTitle("Save Annotations to JSON");
  static const auto saveAnnotDialogFilters = native_dialog::annotationFilters();

  Image* image = appData.image(imageUid);
  if (!image) {
    return;
  }

  auto moveCrosshairsToAnnotationCenter = [&appData, &image](const Annotation& annot) {
    const glm::vec4 subjectCentroid{
      annot.unprojectFromAnnotationPlaneToSubjectPoint(annot.polygon().getCentroid()),
      1.0f};
    const glm::vec4 worldCentroid = image->transformations().worldDef_T_subject() * subjectCentroid;
    appData.state().setWorldCrosshairsPos(glm::vec3{worldCentroid / worldCentroid.w});
  };

  // Finds a view with normal vector matching the annotation plane. (Todo: make this view active.)
  // If none found, make the largest view oblique and align it to the annotation.
  auto alignViewToAnnotationPlane = [&appData, &imageUid, &image, &setViewDirection](const Annotation& annot) {
    const glm::mat3 world_T_subject_invTranspose =
      glm::inverseTranspose(glm::mat3{image->transformations().worldDef_T_subject()});

    const glm::vec3 worldAnnotNormal =
      glm::normalize(world_T_subject_invTranspose * glm::vec3{annot.getSubjectPlaneEquation()});

    // Does the current layout have a view with this orientation?
    const auto viewsWithNormal = appData.windowData().findCurrentViewsWithNormal(worldAnnotNormal);

    if (viewsWithNormal.empty()) {
      const uuids::uuid largestCurrentViewUid = appData.windowData().findLargestCurrentView();

      if (View* view = appData.windowData().getCurrentView(largestCurrentViewUid)) {
        // Rather than check if the plane of the annotation (which, recall is defined in
        // Subject space) is aligned with either an axial, coronal, or sagittal view,
        // we simple set the view to oblique.
        view->setViewType(ViewType::Oblique);
        setViewDirection(largestCurrentViewUid, worldAnnotNormal);

        // Render the image in this view if not currently rendered:
        if (!view->isImageRendered(imageUid)) {
          view->setImageRendered(appData, imageUid, true);
        }

        SPDLOG_TRACE("Changed view {} normal direction to {}", largestCurrentViewUid, glm::to_string(worldAnnotNormal));
      }
      else {
        spdlog::error("Unable to orient a view to the annotation plane");
      }
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

  const auto headerColors = computeHeaderBgAndTextColors(image->settings().borderColor());
  ImGui::PushStyleColor(ImGuiCol_Header, headerColors.first);
  ImGui::PushStyleColor(ImGuiCol_Text, headerColors.second);

  const bool open = activeImageCollapsingHeader(appData.guiData(), isActiveImage, headerName.c_str(), headerFlags);
  ImGui::PopStyleColor(2); // ImGuiCol_Header, ImGuiCol_Text

  if (!open) {
    ImGui::PopID(); // imageUid
    return;
  }

  ImGui::Spacing();

  const auto& annotUids = appData.annotationsForImage(imageUid);
  if (annotUids.empty()) {
    ImGui::Text("This image has no annotations.");
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
    ImGui::TextWrapped("%s", "Switch to Annotate mode and select a view in which to create annotations.");
    ImGui::PopStyleColor();
    if (hasFollowingHeader) {
      ImGui::Spacing();
      ImGui::Separator();
      ImGui::Spacing();
    }
    ImGui::PopID(); // imageUid
    return;
  }

  auto activeAnnotUid = appData.imageToActiveAnnotationUid(imageUid);

  const ImVec4* colors = ImGui::GetStyle().Colors;
  ImGui::PushStyleColor(ImGuiCol_Header, colors[ImGuiCol_ButtonActive]);

  const size_t numLines = std::max(std::min(annotUids.size(), maxNumLines), minNumLines);

  /// @todo Change this into a child window, like for Landmarks.
  /// then do ImGui::SetScrollHereY(1.0f); to put activeAnnot at bottom

  const float listBoxWidth = (activeAnnotUid ? -FLT_MIN : 250.0f);
  const float listBoxHeight = static_cast<float>(numLines) * ImGui::GetTextLineHeightWithSpacing();

  if (ImGui::BeginListBox("##annotList", ImVec2(listBoxWidth, listBoxHeight))) {
    size_t annotIndex = 0;
    for (const auto& annotUid : annotUids) {
      ImGui::PushID(static_cast<int>(annotIndex++));

      Annotation* annot = appData.annotation(annotUid);
      if (!annot) {
        spdlog::error("Null annotation {}", annotUid);
        ImGui::PopID(); // lmGroupIndex
      }

      /// @see Line 2791 of demo:
      /// ImGui::SetScrollHereY(i * 0.25f); // 0.0f:top, 0.5f:center, 1.0f:bottom

      const std::string text = annot->getDisplayName() + " [" + data::getAnnotationSubjectPlaneName(*annot) + "]";
      const bool isSelected = (activeAnnotUid && (annotUid == *activeAnnotUid));

      if (ImGui::Selectable(text.c_str(), isSelected)) {
        // Make the annotation active and move crosshairs to it:
        if (!appData.assignActiveAnnotationUidToImage(imageUid, annotUid)) {
          spdlog::error("Unable to assign active annotation {} to image {}", annotUid, imageUid);
        }

        // Need to synchronize the active annotation change with the highlighting
        // state of annotations.
        ASM::synchronizeAnnotationHighlights();

        if (const Annotation* activeAnnot = appData.annotation(annotUid)) {
          moveCrosshairsToAnnotationCenter(*activeAnnot);
          alignViewToAnnotationPlane(*activeAnnot);

          recenterAllViews(
            doNotRecenterCrosshairs,
            doNotRealignCrosshairs,
            doNotRecenterOnCurrentCrosshairsPosition,
            doNotResetObliqueOrientation,
            doNotResetZoom);
        }
        else {
          spdlog::error("Null active annotation {}", annotUid);
        }
      }

      // Set the initial focus when opening the combo (scrolling + keyboard navigation focus)
      if (isSelected) {
        ImGui::SetItemDefaultFocus();
      }

      ImGui::PopID(); // lmGroupIndex
    }

    ImGui::EndListBox();
  }
  ImGui::PopStyleColor(1); // ImGuiCol_Header

  activeAnnotUid = appData.imageToActiveAnnotationUid(imageUid);
  if (!activeAnnotUid) {
    // If there is no active/selected annotation, then do not render the rest of the header,
    // which shows view properties of the annotation
    if (hasFollowingHeader) {
      ImGui::Spacing();
      ImGui::Separator();
      ImGui::Spacing();
    }
    ImGui::PopID(); // imageUid
    return;
  }

  Annotation* activeAnnot = appData.annotation(*activeAnnotUid);
  if (!activeAnnot) {
    spdlog::error("Null active annotation {}", *activeAnnotUid);
    if (hasFollowingHeader) {
      ImGui::Spacing();
      ImGui::Separator();
      ImGui::Spacing();
    }
    ImGui::PopID(); // imageUid
    return;
  }

  // Annotation display name:
  std::string displayName = activeAnnot->getDisplayName();
  if (ImGui::InputText("Name", &displayName)) {
    activeAnnot->setDisplayName(displayName);
  }
  ImGui::SameLine();
  helpMarker("Edit the name of the annotation");

  ImGui::Text("Layer order: ");

  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));

  ImGui::SameLine();
  if (ImGui::Button(ICON_FK_FAST_BACKWARD)) {
    appData.moveAnnotationToBack(imageUid, *activeAnnotUid);
  }
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Move annotation to backmost layer");
  }

  ImGui::SameLine();
  if (ImGui::Button(ICON_FK_BACKWARD)) {
    appData.moveAnnotationBackwards(imageUid, *activeAnnotUid);
  }
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Move annotation backward in layers (decrease the annotation order)");
  }

  ImGui::SameLine();
  if (ImGui::Button(ICON_FK_FORWARD)) {
    appData.moveAnnotationForwards(imageUid, *activeAnnotUid);
  }
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Move annotation forward in layers (increase the annotation order)");
  }

  ImGui::SameLine();
  if (ImGui::Button(ICON_FK_FAST_FORWARD)) {
    appData.moveAnnotationToFront(imageUid, *activeAnnotUid);
  }
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Move annotation to frontmost layer");
  }

  /*** ImGuiStyleVar_ItemSpacing ***/
  ImGui::PopStyleVar(1);

  // Remove the annotation:
  bool removeAnnot = false;
  static bool doNotAskAagain = false;

  ImGui::Spacing();
  const bool clickedRemoveButton = ImGui::Button(removeAnnotButtonText.c_str());
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Remove the annotation. (The saved file on disk will not be deleted.)");
  }

  if (clickedRemoveButton) {
    if (!doNotAskAagain && !ImGui::IsPopupOpen("Remove Annotation")) {
      ImGui::OpenPopup("Remove Annotation", ImGuiWindowFlags_AlwaysAutoResize);
    }
    else if (doNotAskAagain) {
      removeAnnot = true;
    }
  }

  // Fill the active segmentation with the annotation:
  if (activeAnnot->isClosed() && !activeAnnot->isSmoothed()) {
    ImGui::SameLine();
    if (ImGui::Button(fillAnnotButtonText.c_str())) {
      if (paintActiveSegmentationWithActivePolygon) {
        paintActiveSegmentationWithActivePolygon();
      }
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Fill the active image segmentation with the selected annotation polygon");
    }
  }

  const ImVec2 center(ImGui::GetIO().DisplaySize.x * 0.5f, ImGui::GetIO().DisplaySize.y * 0.5f);

  ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

  if (ImGui::BeginPopupModal("Remove Annotation", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::Text("Are you sure that you want to remove annotation '%s'?", activeAnnot->getDisplayName().c_str());
    ImGui::Separator();

    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
    ImGui::Checkbox("Do not ask again", &doNotAskAagain);
    ImGui::PopStyleVar();

    if (ImGui::Button("Yes")) {
      removeAnnot = true;
      ImGui::CloseCurrentPopup();
    }
    ImGui::SetItemDefaultFocus();

    ImGui::SameLine();
    if (ImGui::Button("No")) {
      removeAnnot = false;
      ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
  }

  if (removeAnnot) {
    if (appData.removeAnnotation(*activeAnnotUid)) {
      spdlog::info("Removed annotation {}", *activeAnnotUid);
      ImGui::PopID(); // imageUid
      return;
    }
    else {
      spdlog::error("Unable to remove annotation {}", *activeAnnotUid);
    }
  }

  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Spacing();

  // Boundary:
  bool isClosed = activeAnnot->isClosed();
  if (ImGui::RadioButton("Open", !isClosed)) {
    activeAnnot->setClosed(false);
  }

  ImGui::SameLine();
  if (ImGui::RadioButton("Closed boundary", isClosed)) {
    activeAnnot->setClosed(true);
  }
  ImGui::SameLine();
  helpMarker("Set whether the annotation polygon boundary is open or closed");

  // Smoothing:
  bool smooth = activeAnnot->isSmoothed();
  if (ImGui::Checkbox("Smooth", &smooth)) {
    activeAnnot->setSmoothed(smooth);
  }
  ImGui::SameLine();
  helpMarker("Smooth the annotation boundary");

  if (activeAnnot->isSmoothed()) {
    float smoothing = activeAnnot->getSmoothingFactor();
    if (mySliderF32("Smoothing", &smoothing, 0.0f, 0.2f, "%0.2f")) {
      activeAnnot->setSmoothingFactor(smoothing);
    }
    ImGui::SameLine();
    helpMarker("Smoothing factor");
  }

  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Spacing();

  // Visibility checkbox:
  bool annotVisible = activeAnnot->isVisible();
  if (ImGui::Checkbox("Visible", &annotVisible)) {
    activeAnnot->setVisible(annotVisible);
  }
  ImGui::SameLine();
  helpMarker("Show/hide the annotation");

  // Show vertices checkbox:
  if (!appData.renderData().m_globalAnnotationParams.hidePolygonVertices) {
    bool showVertices = activeAnnot->getVertexVisibility();
    if (ImGui::Checkbox("Show vertices", &showVertices)) {
      activeAnnot->setVertexVisibility(showVertices);
    }
    ImGui::SameLine();
    helpMarker("Show/hide the annotation vertices");
  }

  // Filled checkbox:
  if (activeAnnot->isClosed()) {
    bool filled = activeAnnot->isFilled();
    if (ImGui::Checkbox("Filled", &filled)) {
      activeAnnot->setFilled(filled);
    }
    ImGui::SameLine();
    helpMarker("Fill the annotation interior");
  }

  // Opacity slider:
  float annotOpacity = activeAnnot->getOpacity();
  if (mySliderF32("Opacity", &annotOpacity, 0.0f, 1.0f)) {
    activeAnnot->setOpacity(annotOpacity);
  }
  ImGui::SameLine();
  helpMarker("Overall annotation opacity");

  // Line stroke thickness:
  float annotThickness = activeAnnot->getLineThickness();
  if (ImGui::InputFloat("Line thickness", &annotThickness, 0.1f, 1.0f, "%0.2f")) {
    if (annotThickness >= 0.0f) {
      activeAnnot->setLineThickness(annotThickness);
    }
  }
  ImGui::SameLine();
  helpMarker("Annotation line thickness");

  // Line color:
  glm::vec4 annotLineColor = activeAnnot->getLineColor();
  if (ImGui::ColorEdit4("Line color", glm::value_ptr(annotLineColor), annotColorEditFlags)) {
    activeAnnot->setLineColor(annotLineColor);
    activeAnnot->setVertexColor(annotLineColor);
  }
  ImGui::SameLine();
  helpMarker("Annotation line color");

  const bool showFillColorButton = (activeAnnot->isClosed() && activeAnnot->isFilled());

  if (showFillColorButton) {
    ImGui::SameLine();
    if (ImGui::Button(ICON_FK_LEVEL_DOWN)) {
      glm::vec4 fillColor{annotLineColor};
      fillColor.a = activeAnnot->getFillColor().a;
      activeAnnot->setFillColor(fillColor);
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Match fill color to line color");
    }

    // Fill color:
    glm::vec4 annotFillColor = activeAnnot->getFillColor();
    if (ImGui::ColorEdit4("Fill color", glm::value_ptr(annotFillColor), annotColorEditFlags)) {
      activeAnnot->setFillColor(annotFillColor);
    }
    ImGui::SameLine();
    helpMarker("Annotation fill color");

    ImGui::SameLine();
    if (ImGui::Button(ICON_FK_LEVEL_UP)) {
      glm::vec4 lineColor{annotFillColor};
      lineColor.a = activeAnnot->getLineColor().a;
      activeAnnot->setLineColor(lineColor);
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Match line color to fill color");
    }
  }

  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Spacing();

  // Plane normal vector and offset:
  ImGui::Text("Annotation plane (Subject space):");

  const char* coordFormat = appData.guiData().m_coordsPrecisionFormat.c_str();

  glm::vec4 annotPlaneEq = activeAnnot->getSubjectPlaneEquation();
  ImGui::InputFloat3("Normal", glm::value_ptr(annotPlaneEq), coordFormat);
  ImGui::SameLine();
  helpMarker("Annotation plane normal vector (x, y, z) in image Subject space");

  ImGui::InputFloat("Offset (mm)", &annotPlaneEq[3], 0.0f, 0.0f, coordFormat);
  ImGui::SameLine();
  helpMarker("Offset distance (mm) of annotation plane from the image Subject space origin");

  // Number of vertices
  static constexpr size_t OUTER_BOUNDARY = 0;
  if (activeAnnot->polygon().numBoundaries() > 0) {
    ImGui::Spacing();
    ImGui::Text("Polygon has %ld vertices", activeAnnot->polygon().getBoundaryVertices(OUTER_BOUNDARY).size());
  }

  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Spacing();

  // Annotation file name:
  std::string fileName = activeAnnot->getFileName().string();
  ImGui::InputText("File", &fileName, ImGuiInputTextFlags_ReadOnly);
  ImGui::SameLine();
  helpMarker("File storing the annotation");

  if (!annotUids.empty()) {
    // Save annotations to disk:
    const auto selectedFile =
      ImGui::renderFileButtonDialogAndWindow(saveAnnotButtonText.c_str(), saveAnnotDialogTitle, saveAnnotDialogFilters);

    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Save annotations to disk");
    }

    if (selectedFile) {
      // Add all annotations belonging to this image to a json structure,
      // then save the json to disk.
      nlohmann::json j;

      for (const auto& annotUid : appData.annotationsForImage(imageUid)) {
        if (const Annotation* annot = appData.annotation(annotUid)) {
          serialize::appendAnnotationToJson(*annot, j);
        }
      }

      if (serialize::saveToJsonFile(j, *selectedFile)) {
        spdlog::info("Saved annotations for image {} to JSON file {}", imageUid, *selectedFile);
        for (const auto& annotUid : appData.annotationsForImage(imageUid)) {
          if (Annotation* annot = appData.annotation(annotUid)) {
            annot->setFileName(*selectedFile);
            annot->markClean();
          }
        }
      }
      else {
        spdlog::error("Error saving annotation to SVG file {}", *selectedFile);
      }
    }
  }

  if (hasFollowingHeader) {
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
  }
  ImGui::PopID(); /** PopID imageUid **/
}
