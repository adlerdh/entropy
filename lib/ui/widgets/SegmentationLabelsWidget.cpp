#include "ui/widgets/SegmentationLabelsWidget.h"

#include "logic/app/ParcellationLabelTable.h"

#include <IconsForkAwesome.h>
#include <glm/vec4.hpp>
#include <glm/gtc/type_precision.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <imgui/imgui.h>
#include <imgui/misc/cpp/imgui_stdlib.h>

#include <cstdint>
#include <cstdio>
#include <string>

namespace
{
enum SegmentationLabelColumn : int
{
  Visibility2D,
  Visibility3D,
  Index,
  Label,
  Cutaway,
  Count
};
} // namespace

void renderSegLabelsChildWindow(
  std::size_t tableIndex,
  ParcellationLabelTable* labelTable,
  std::size_t selectedLabel,
  const std::function<void(std::size_t labelIndex)>& selectLabel,
  const std::function<void(std::size_t tableIndex)>& updateLabelColorTableTexture,
  const std::function<void(std::size_t labelIndex)>& moveCrosshairsToSegLabelCentroid)
{
  static const std::string sk_showAll2d = std::string(ICON_FK_EYE) + " 2D";
  static const std::string sk_hideAll2d = std::string(ICON_FK_EYE_SLASH) + " 2D";
  static const std::string sk_showAll3d = std::string(ICON_FK_EYE) + " 3D";
  static const std::string sk_hideAll3d = std::string(ICON_FK_EYE_SLASH) + " 3D";
  static const std::string sk_addNew = std::string(ICON_FK_PLUS) + " Add new";

  static const ImGuiColorEditFlags sk_colorEditFlags =
    ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaPreviewHalf | ImGuiColorEditFlags_AlphaBar |
    ImGuiColorEditFlags_Uint8 | ImGuiColorEditFlags_DisplayRGB | ImGuiColorEditFlags_DisplayHSV |
    ImGuiColorEditFlags_DisplayHex;

  if (!labelTable) {
    return;
  }

  const bool childVisible = ImGui::BeginChild("##labelChild", ImVec2(0.0f, 375.0f), true, ImGuiWindowFlags_MenuBar);

  if (!childVisible) {
    ImGui::EndChild();
    return;
  }

  bool scrollToBottomOfLmList = false;

  if (ImGui::BeginMenuBar()) {
    if (ImGui::MenuItem(sk_addNew.c_str())) {
      labelTable->addLabels(1);
      updateLabelColorTableTexture(tableIndex);

      // Scroll child window to the end of the list of landmarks
      scrollToBottomOfLmList = true;
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Add a new segmentation");
    }

    if (ImGui::MenuItem(sk_showAll2d.c_str())) {
      for (std::size_t i = 0; i < labelTable->numLabels(); ++i) {
        labelTable->setVisible(i, i != 0);
      }
      updateLabelColorTableTexture(tableIndex);
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Show all segmentations in 2D views");
    }

    if (ImGui::MenuItem(sk_hideAll2d.c_str())) {
      for (std::size_t i = 0; i < labelTable->numLabels(); ++i) {
        labelTable->setVisible(i, false);
      }
      updateLabelColorTableTexture(tableIndex);
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Hide all segmentations in 2D views");
    }

    if (ImGui::MenuItem(sk_showAll3d.c_str())) {
      for (std::size_t i = 0; i < labelTable->numLabels(); ++i) {
        labelTable->setShowMesh(i, i != 0);
      }
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Show all segmentations in 3D views");
    }

    if (ImGui::MenuItem(sk_hideAll3d.c_str())) {
      for (std::size_t i = 0; i < labelTable->numLabels(); ++i) {
        labelTable->setShowMesh(i, false);
      }
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Hide all segmentations in 3D views");
    }

    ImGui::EndMenuBar();
  }

  const ImGuiStyle& style = ImGui::GetStyle();
  const float compactColumnWidth = ImGui::GetFrameHeight() + 2.0f * style.CellPadding.x;
  const float cutawayColumnWidth = ImGui::CalcTextSize("Cutaway").x + 2.0f * style.CellPadding.x;
  constexpr ImGuiTableFlags tableFlags = ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_BordersInnerV |
                                         ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY |
                                         ImGuiTableFlags_SizingStretchProp;

  if (!ImGui::BeginTable("##segmentationLabels", SegmentationLabelColumn::Count, tableFlags, ImVec2{0.0f, 0.0f})) {
    ImGui::EndChild();
    return;
  }

  ImGui::TableSetupScrollFreeze(0, 1);
  ImGui::TableSetupColumn("2D", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoResize, compactColumnWidth);
  ImGui::TableSetupColumn("3D", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoResize, compactColumnWidth);
  ImGui::TableSetupColumn("Index");
  ImGui::TableSetupColumn("Label");
  ImGui::TableSetupColumn(
    "Cutaway",
    ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoResize,
    cutawayColumnWidth);

  ImGui::TableNextRow(ImGuiTableRowFlags_Headers);

  for (int column = 0; column < SegmentationLabelColumn::Count; ++column) {
    ImGui::TableSetColumnIndex(column);
    ImGui::TableHeader(ImGui::TableGetColumnName(column));
    if (ImGui::IsItemHovered()) {
      if (column == SegmentationLabelColumn::Visibility2D) {
        ImGui::SetTooltip("Show this segmentation in 2D views");
      }
      else if (column == SegmentationLabelColumn::Visibility3D) {
        ImGui::SetTooltip("Show this segmentation in 3D views");
      }
      else if (column == SegmentationLabelColumn::Index) {
        ImGui::SetTooltip("Label index");
      }
      else if (column == SegmentationLabelColumn::Label) {
        ImGui::SetTooltip("Label region");
      }
      else if (column == SegmentationLabelColumn::Cutaway) {
        ImGui::SetTooltip(
          "Include this label's mesh in Cutaway. Cutaway removes the viewer-facing octant at the crosshairs.");
      }
    }
  }

  for (std::size_t i = 0; i < labelTable->numLabels(); ++i) {
    char labelIndexBuffer[32];
    snprintf(labelIndexBuffer, 32, "%03zu", i);

    bool labelVisible = labelTable->getVisible(i);
    bool labelShowMesh = labelTable->getShowMesh(i);
    bool labelIncludeInCutaway = labelTable->getIncludeInCutaway(i);
    std::string labelName = labelTable->getName(i);

    // ImGui::ColorEdit represents color as non-premultiplied colors
    glm::vec4 labelColor = glm::vec4{labelTable->getColor(i), labelTable->getAlpha(i)} / 255.0f;

    ImGui::PushID(static_cast<int>(i)); /*** PushID i ***/
    ImGui::TableNextRow();

    ImGui::TableSetColumnIndex(SegmentationLabelColumn::Visibility2D);
    const ImVec2 rowStart = ImGui::GetCursorScreenPos();
    if (ImGui::Selectable(
          "##selectLabel",
          selectedLabel == i,
          ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap,
          ImVec2{0.0f, ImGui::GetFrameHeight()}))
    {
      selectLabel(i);
    }
    ImGui::SetCursorScreenPos(rowStart);

    if (ImGui::Checkbox("##labelVisible", &labelVisible)) {
      labelTable->setVisible(i, labelVisible);
      updateLabelColorTableTexture(tableIndex);
    }
    if (ImGui::IsItemClicked()) {
      selectLabel(i);
    }

    ImGui::TableSetColumnIndex(SegmentationLabelColumn::Visibility3D);
    if (ImGui::Checkbox("##labelShowMesh", &labelShowMesh)) {
      labelTable->setShowMesh(i, labelShowMesh);
    }
    if (ImGui::IsItemClicked()) {
      selectLabel(i);
    }

    ImGui::TableSetColumnIndex(SegmentationLabelColumn::Index);
    if (ImGui::ColorEdit4("##labelColor", glm::value_ptr(labelColor), sk_colorEditFlags)) {
      labelTable->setColor(i, glm::u8vec3{255.0f * labelColor});
      labelTable->setAlpha(i, static_cast<uint8_t>(255.0f * labelColor.a));
      updateLabelColorTableTexture(tableIndex);
    }
    if (ImGui::IsItemClicked()) {
      selectLabel(i);
    }
    ImGui::SameLine();
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(labelIndexBuffer);

    ImGui::SameLine();
    if (ImGui::Button(ICON_FK_HAND_O_UP)) {
      selectLabel(i);
      moveCrosshairsToSegLabelCentroid(i);

      /// @todo Should the views recenter? This done when moving crosshairs to a landmark.

      // With second argument set to true, this function centers all views on the crosshairs.
      // That way, views show the crosshairs even if they were not in the original view bounds.
      //            recenterAllViews( false, true, false );
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Move crosshairs to segmentation centroid");
    }

    ImGui::TableSetColumnIndex(SegmentationLabelColumn::Label);
    ImGui::SetNextItemWidth(-1.0f);
    if (ImGui::InputText("##labelName", &labelName)) {
      labelTable->setName(i, labelName);
    }
    if (ImGui::IsItemClicked()) {
      selectLabel(i);
    }

    ImGui::TableSetColumnIndex(SegmentationLabelColumn::Cutaway);
    if (ImGui::Checkbox("##labelIncludeInCutaway", &labelIncludeInCutaway)) {
      labelTable->setIncludeInCutaway(i, labelIncludeInCutaway);
    }
    if (ImGui::IsItemClicked()) {
      selectLabel(i);
    }

    if (scrollToBottomOfLmList) {
      if (i == (labelTable->numLabels() - 1)) {
        ImGui::SetScrollHereY(1.0f);
        scrollToBottomOfLmList = false;
      }
    }

    ImGui::PopID(); /*** PopID i ***/
  }

  ImGui::EndTable();

  ImGui::EndChild();
}
