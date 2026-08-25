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

void renderSegLabelsChildWindow(
  std::size_t tableIndex,
  ParcellationLabelTable* labelTable,
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
  const float indexColumnWidth = 2.0f * ImGui::GetFrameHeight() + 2.0f * style.ItemInnerSpacing.x +
                                 ImGui::CalcTextSize("000").x + 2.0f * style.CellPadding.x;
  constexpr ImGuiTableFlags tableFlags = ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_BordersInnerV |
                                         ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY |
                                         ImGuiTableFlags_SizingFixedFit;

  if (!ImGui::BeginTable("##segmentationLabels", 4, tableFlags, ImVec2{0.0f, 0.0f})) {
    ImGui::EndChild();
    return;
  }

  ImGui::TableSetupScrollFreeze(0, 1);
  ImGui::TableSetupColumn("2D", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoResize, compactColumnWidth);
  ImGui::TableSetupColumn("3D", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoResize, compactColumnWidth);
  ImGui::TableSetupColumn("Index", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoResize, indexColumnWidth);
  ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthStretch, 1.0f);
  ImGui::TableNextRow(ImGuiTableRowFlags_Headers);
  for (int column = 0; column < 4; ++column) {
    ImGui::TableSetColumnIndex(column);
    ImGui::TableHeader(ImGui::TableGetColumnName(column));
    if (ImGui::IsItemHovered()) {
      if (column == 0) {
        ImGui::SetTooltip("Show this segmentation in 2D views");
      }
      else if (column == 1) {
        ImGui::SetTooltip("Show this segmentation in 3D views");
      }
    }
  }

  for (std::size_t i = 0; i < labelTable->numLabels(); ++i) {
    char labelIndexBuffer[32];
    snprintf(labelIndexBuffer, 32, "%03zu", i);

    bool labelVisible = labelTable->getVisible(i);
    bool labelShowMesh = labelTable->getShowMesh(i);
    std::string labelName = labelTable->getName(i);

    // ImGui::ColorEdit represents color as non-premultiplied colors
    glm::vec4 labelColor = glm::vec4{labelTable->getColor(i), labelTable->getAlpha(i)} / 255.0f;

    ImGui::PushID(static_cast<int>(i)); /*** PushID i ***/
    ImGui::TableNextRow();

    ImGui::TableSetColumnIndex(0);
    if (ImGui::Checkbox("##labelVisible", &labelVisible)) {
      labelTable->setVisible(i, labelVisible);
      updateLabelColorTableTexture(tableIndex);
    }

    ImGui::TableSetColumnIndex(1);
    if (ImGui::Checkbox("##labelShowMesh", &labelShowMesh)) {
      labelTable->setShowMesh(i, labelShowMesh);
    }

    ImGui::TableSetColumnIndex(2);
    if (ImGui::ColorEdit4("##labelColor", glm::value_ptr(labelColor), sk_colorEditFlags)) {
      labelTable->setColor(i, glm::u8vec3{255.0f * labelColor});
      labelTable->setAlpha(i, static_cast<uint8_t>(255.0f * labelColor.a));
      updateLabelColorTableTexture(tableIndex);
    }
    ImGui::SameLine();
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(labelIndexBuffer);

    ImGui::SameLine();
    if (ImGui::Button(ICON_FK_HAND_O_UP)) {
      moveCrosshairsToSegLabelCentroid(i);

      /// @todo Should the views recenter? This done when moving crosshairs to a landmark.

      // With second argument set to true, this function centers all views on the crosshairs.
      // That way, views show the crosshairs even if they were not in the original view bounds.
      //            recenterAllViews( false, true, false );
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Move crosshairs to segmentation centroid");
    }

    ImGui::TableSetColumnIndex(3);
    ImGui::SetNextItemWidth(-1.0f);
    if (ImGui::InputText("##labelName", &labelName)) {
      labelTable->setName(i, labelName);
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
