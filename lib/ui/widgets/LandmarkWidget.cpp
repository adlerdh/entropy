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

void renderLandmarkChildWindow(
  const AppData& appData,
  const ImageTransformations& imageTransformations,
  LandmarkGroup* activeLmGroup,
  const glm::vec3& worldCrosshairsPos,
  const std::function<void(const glm::vec3& worldCrosshairsPos)>& setWorldCrosshairsPos,
  const AllViewsRecenterType& recenterAllViews)
{
  static const std::string sk_addNew = std::string(ICON_FK_PLUS) + " Add new";
  static const std::string sk_showAll = std::string(ICON_FK_EYE) + " Show all";
  static const std::string sk_hideAll = std::string(ICON_FK_EYE_SLASH) + " Hide all";

  static const ImGuiColorEditFlags sk_colorEditFlags = ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_PickerHueBar |
                                                       ImGuiColorEditFlags_DisplayRGB | ImGuiColorEditFlags_DisplayHSV |
                                                       ImGuiColorEditFlags_DisplayHex | ImGuiColorEditFlags_Uint8 |
                                                       ImGuiColorEditFlags_InputRGB;

  static const auto sk_hueMinMax = std::make_pair(0.0f, 360.0f);
  static const auto sk_satMinMax = std::make_pair(0.3f, 1.0f);
  static const auto sk_valMinMax = std::make_pair(0.3f, 1.0f);

  const char* coordFormat = appData.guiData().m_coordsPrecisionFormat.c_str();

  std::map<size_t, PointRecord<glm::vec3> >& points = activeLmGroup->getPoints();

  const bool childVisible =
    ImGui::BeginChild("", ImVec2(375, 300), true, ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_HorizontalScrollbar);

  if (!childVisible) {
    ImGui::EndChild();
    return;
  }

  bool scrollToBottomOfLmList = false;

  if (ImGui::BeginMenuBar()) {
    /// @todo Pull this function out of here.
    /// Will need to add concept of "active image or landmarking".
    if (ImGui::MenuItem(sk_addNew.c_str())) {
      // Add new landmark at crosshairs position in the correct space
      const glm::mat4 landmark_T_world = (activeLmGroup->getInVoxelSpace()) ? imageTransformations.pixel_T_worldDef()
                                                                            : imageTransformations.subject_T_worldDef();

      const glm::vec4 lmPos = landmark_T_world * glm::vec4{worldCrosshairsPos, 1.0f};

      PointRecord<glm::vec3> pointRec{glm::vec3{lmPos / lmPos.w}};

      // Assign the new point a random color, seeded by its index
      const size_t newIndex = (activeLmGroup->getPoints().empty()) ? 0u : activeLmGroup->maxIndex() + 1;

      const auto colors =
        math::generateRandomHsvSamples(1, sk_hueMinMax, sk_satMinMax, sk_valMinMax, static_cast<uint32_t>(newIndex));

      if (!colors.empty()) {
        pointRec.setColor(glm::rgbColor(colors[0]));
      }

      activeLmGroup->addPoint(newIndex, pointRec);

      // Scroll child window to the end of the list of landmarks
      scrollToBottomOfLmList = true;
    }

    if (ImGui::MenuItem(sk_showAll.c_str())) {
      for (auto& p : points) {
        p.second.setVisibility(true);
      }
    }

    if (ImGui::MenuItem(sk_hideAll.c_str())) {
      for (auto& p : points) {
        p.second.setVisibility(false);
      }
    }

    ImGui::EndMenuBar();
  }

  char pointIndexBuffer[8];

  for (auto& p : points) {
    const size_t pointIndex = p.first;
    auto& point = p.second;

    snprintf(pointIndexBuffer, 8, "%03zu", pointIndex);

    bool pointVisible = point.getVisibility();
    std::string pointName = point.getName();
    glm::vec3 pointColor = point.getColor();
    glm::vec3 pointPos = point.getPosition();

    ImGui::PushID(static_cast<int>(pointIndex)); /*** PushID pointIndex ***/

    if (ImGui::Checkbox(pointIndexBuffer, &pointVisible)) {
      point.setVisibility(pointVisible);
    }

    if (!activeLmGroup->getColorOverride()) {
      ImGui::SameLine();
      if (ImGui::ColorEdit3("", glm::value_ptr(pointColor), sk_colorEditFlags)) {
        point.setColor(pointColor);
      }
    }

    ImGui::SameLine();

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(2.0f, 4.0f));

    if (ImGui::Button(ICON_FK_HAND_O_UP)) {
      const glm::mat4 world_T_landmark = (activeLmGroup->getInVoxelSpace()) ? imageTransformations.worldDef_T_pixel()
                                                                            : imageTransformations.worldDef_T_subject();

      const glm::vec4 worldPos = world_T_landmark * glm::vec4{pointPos, 1.0f};
      setWorldCrosshairsPos(glm::vec3{worldPos / worldPos.w});

      // With second argument set to true, this function centers all views on the crosshairs.
      // That way, views show the crosshairs even if they were not in the original view bounds.
      recenterAllViews(false, false, true, false, false);
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Move crosshairs to landmark and center views on landmark");
    }

    ImGui::SameLine();
    if (ImGui::Button(ICON_FK_CROSSHAIRS)) {
      const glm::mat4 landmark_T_world = (activeLmGroup->getInVoxelSpace()) ? imageTransformations.pixel_T_worldDef()
                                                                            : imageTransformations.subject_T_worldDef();

      const glm::vec4 lmPos = landmark_T_world * glm::vec4{worldCrosshairsPos, 1.0f};

      point.setPosition(glm::vec3{lmPos / lmPos.w});
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Set landmark to the current crosshairs position");
    }

    ImGui::SameLine();
    if (ImGui::Button(ICON_FK_TIMES)) {
      if (activeLmGroup->removePoint(pointIndex)) {
        // The point was removed, so skip rendering it
        ImGui::PopID(); // pointIndex
        ImGui::EndChild();
        return;
      }
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Delete landmark");
    }

    // ImGuiStyleVar_ItemSpacing
    ImGui::PopStyleVar();

    if (activeLmGroup->getRenderLandmarkNames()) {
      // Edit the name if they are visible
      ImGui::SameLine();
      ImGui::PushItemWidth(100.0f);
      if (ImGui::InputText("##pointName", &pointName)) {
        point.setName(pointName);
      }
      ImGui::PopItemWidth();
      if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Landmark name");
      }
    }

    ImGui::SameLine();

    ImGui::PushStyleVar(ImGuiStyleVar_ItemInnerSpacing, ImVec2(1.0f, 4.0f));
    ImGui::PushItemWidth(200.0f);
    if (ImGui::InputFloat3("##pointPos", glm::value_ptr(pointPos), coordFormat, 0)) {
      point.setPosition(pointPos);
    }

    if (ImGui::IsItemHovered()) {
      if (activeLmGroup->getInVoxelSpace()) {
        ImGui::SetTooltip("(x, y, z) voxel position");
      }
      else {
        ImGui::SetTooltip("(x, y, z) physical position (mm)");
      }
    }

    ImGui::PopItemWidth();
    ImGui::PopStyleVar(); // ImGuiStyleVar_ItemInnerSpacing

    if (scrollToBottomOfLmList) {
      if (pointIndex == (points.size() - 1)) {
        ImGui::SetScrollHereY(1.0f);
        scrollToBottomOfLmList = false;
      }
    }

    ImGui::PopID(); /*** PopID pointIndex ***/
  }

  ImGui::EndChild();
}
