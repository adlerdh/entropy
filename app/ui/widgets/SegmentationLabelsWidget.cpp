#include "ui/widgets/Widgets.h"
#include "ui/Helpers.h"
#include "ui/ImGuiCustomControls.h"

#include "common/MathFuncs.h"

#include "image/ImageColorMap.h"
#include "image/ImageTransformations.h"

#include "logic/app/Data.h"

#include <IconsForkAwesome.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_precision.hpp>
#include <glm/gtc/type_ptr.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/color_space.hpp>
#include <imgui/imgui.h>
#include <imgui/misc/cpp/imgui_stdlib.h>

#include <spdlog/fmt/ostr.h>
#include <spdlog/spdlog.h>

#include <string>

void renderSegLabelsChildWindow(
  std::size_t tableIndex,
  ParcellationLabelTable* labelTable,
  const std::function<void(std::size_t tableIndex)>& updateLabelColorTableTexture,
  const std::function<void(std::size_t labelIndex)>& moveCrosshairsToSegLabelCentroid)
{
  static const std::string sk_showAll = std::string(ICON_FK_EYE) + " Show all";
  static const std::string sk_hideAll = std::string(ICON_FK_EYE_SLASH) + " Hide all";
  static const std::string sk_addNew = std::string(ICON_FK_PLUS) + " Add new";

  static const ImGuiColorEditFlags sk_colorEditFlags =
    ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaPreviewHalf | ImGuiColorEditFlags_AlphaBar |
    ImGuiColorEditFlags_Uint8 | ImGuiColorEditFlags_DisplayRGB | ImGuiColorEditFlags_DisplayHSV |
    ImGuiColorEditFlags_DisplayHex;

  if (!labelTable) {
    return;
  }

  const bool childVisible = ImGui::BeginChild(
    "##labelChild",
    ImVec2(0.0f, 250.0f),
    true,
    ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_HorizontalScrollbar);

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

    if (ImGui::MenuItem(sk_showAll.c_str())) {
      for (std::size_t i = 0; i < labelTable->numLabels(); ++i) {
        labelTable->setVisible(i, true);
      }
      updateLabelColorTableTexture(tableIndex);
    }

    if (ImGui::MenuItem(sk_hideAll.c_str())) {
      for (std::size_t i = 0; i < labelTable->numLabels(); ++i) {
        labelTable->setVisible(i, false);
      }
      updateLabelColorTableTexture(tableIndex);
    }

    ImGui::EndMenuBar();
  }

  for (std::size_t i = 0; i < labelTable->numLabels(); ++i) {
    char labelIndexBuffer[32];
    snprintf(labelIndexBuffer, 32, "%03zu", i);

    bool labelVisible = labelTable->getVisible(i);
    std::string labelName = labelTable->getName(i);

    // ImGui::ColorEdit represents color as non-premultiplied colors
    glm::vec4 labelColor = glm::vec4{labelTable->getColor(i), labelTable->getAlpha(i)} / 255.0f;

    ImGui::PushID(static_cast<int>(i)); /*** PushID i ***/

    if (ImGui::Checkbox("##labelVisible", &labelVisible)) {
      labelTable->setVisible(i, labelVisible);
      updateLabelColorTableTexture(tableIndex);
    }

    ImGui::SameLine();
    if (ImGui::ColorEdit4(labelIndexBuffer, glm::value_ptr(labelColor), sk_colorEditFlags)) {
      labelTable->setColor(i, glm::u8vec3{255.0f * labelColor});
      labelTable->setAlpha(i, static_cast<uint8_t>(255.0f * labelColor.a));
      updateLabelColorTableTexture(tableIndex);
    }

    ImGui::SameLine();
    if (ImGui::Button(ICON_FK_HAND_O_UP)) {
      moveCrosshairsToSegLabelCentroid(i);

      /// @todo Should the views recenter? This done when moving crosshairs to a landmark.

      // With second argument set to true, this function centers all views on the crosshairs.
      // That way, views show the crosshairs even if they were not in the original view bounds.
      //            recenterAllViews( false, true, false );
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Move crosshairs to segmentation label centroid");
    }

    ImGui::SameLine();

    // ImGui::PushItemWidth( 175.0f );
    ImGui::PushItemWidth(-1);
    if (ImGui::InputText("##labelName", &labelName)) {
      labelTable->setName(i, labelName);
    }
    ImGui::PopItemWidth();

    if (scrollToBottomOfLmList) {
      if (i == (labelTable->numLabels() - 1)) {
        ImGui::SetScrollHereY(1.0f);
        scrollToBottomOfLmList = false;
      }
    }

    ImGui::PopID(); /*** PopID i ***/
  }

  ImGui::EndChild();
}
