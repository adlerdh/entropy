#include "ui/Widgets.h"
#include "ui/Helpers.h"
#include "ui/ImGuiCustomControls.h"

#include "common/MathFuncs.h"

#include "image/ImageColorMap.h"
#include "image/ImageTransformations.h"

#include "logic/app/Data.h"

#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>
#include <imgui/misc/cpp/imgui_stdlib.h>

#include <IconFontCppHeaders/IconsForkAwesome.h>

#include <glm/glm.hpp>
#include <glm/gtc/type_precision.hpp>
#include <glm/gtc/type_ptr.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/color_space.hpp>

#include <spdlog/fmt/ostr.h>
#include <spdlog/spdlog.h>

#include <string>

void renderActiveImageSelectionCombo(
  size_t numImages,
  const std::function<std::pair<const char*, const char*>(std::size_t index)>&
    getImageDisplayAndFileName,
  const std::function<std::size_t(void)>& getActiveImageIndex,
  const std::function<void(std::size_t)>& setActiveImageIndex,
  bool showText
)
{
  const std::size_t activeIndex = getActiveImageIndex();

  if (activeIndex >= numImages)
  {
    spdlog::error("Invalid active image index");
    return;
  }

  const std::string nameString = (showText) ? "Active image###imageSelectionCombo"
                                            : "###imageSelectionCombo";

  //    ImGui::Text( "Active image:" );

  //    ImGui::PushItemWidth( -1 );
  if (ImGui::BeginCombo(nameString.c_str(), getImageDisplayAndFileName(activeIndex).first))
  {
    for (std::size_t i = 0; i < numImages; ++i)
    {
      const auto displayAndFileName = getImageDisplayAndFileName(i);
      const bool isSelected = (i == activeIndex);

      ImGui::PushID(i); // needed in case two images have the same display name
      if (ImGui::Selectable(displayAndFileName.first, isSelected))
      {
        setActiveImageIndex(i);
      }
      ImGui::PopID();

      if (isSelected)
      {
        ImGui::SetItemDefaultFocus();
      }
    }

    ImGui::EndCombo();
  }
  //    ImGui::PopItemWidth();

  ImGui::SameLine();
  helpMarker("Select the image that is being actively transformed, adjusted, or segmented");
}

void renderSegLabelsChildWindow(
  std::size_t tableIndex,
  ParcellationLabelTable* labelTable,
  const std::function<void(std::size_t tableIndex)>& updateLabelColorTableTexture,
  const std::function<void(std::size_t labelIndex)>& moveCrosshairsToSegLabelCentroid
)
{
  static const std::string sk_showAll = std::string(ICON_FK_EYE) + " Show all";
  static const std::string sk_hideAll = std::string(ICON_FK_EYE_SLASH) + " Hide all";
  static const std::string sk_addNew = std::string(ICON_FK_PLUS) + " Add new";

  static const ImGuiColorEditFlags sk_colorEditFlags = ImGuiColorEditFlags_NoInputs
                                                       | ImGuiColorEditFlags_AlphaPreviewHalf
                                                       | ImGuiColorEditFlags_AlphaBar
                                                       | ImGuiColorEditFlags_Uint8
                                                       | ImGuiColorEditFlags_DisplayRGB
                                                       | ImGuiColorEditFlags_DisplayHSV
                                                       | ImGuiColorEditFlags_DisplayHex;

  if (!labelTable)
  {
    return;
  }

  const bool childVisible = ImGui::BeginChild(
    "##labelChild",
    ImVec2(0.0f, 250.0f),
    true,
    ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_HorizontalScrollbar
  );

  if (!childVisible)
  {
    ImGui::EndChild();
    return;
  }

  bool scrollToBottomOfLmList = false;

  if (ImGui::BeginMenuBar())
  {
    if (ImGui::MenuItem(sk_addNew.c_str()))
    {
      labelTable->addLabels(1);
      updateLabelColorTableTexture(tableIndex);

      // Scroll child window to the end of the list of landmarks
      scrollToBottomOfLmList = true;
    }

    if (ImGui::MenuItem(sk_showAll.c_str()))
    {
      for (std::size_t i = 0; i < labelTable->numLabels(); ++i)
      {
        labelTable->setVisible(i, true);
      }
      updateLabelColorTableTexture(tableIndex);
    }

    if (ImGui::MenuItem(sk_hideAll.c_str()))
    {
      for (std::size_t i = 0; i < labelTable->numLabels(); ++i)
      {
        labelTable->setVisible(i, false);
      }
      updateLabelColorTableTexture(tableIndex);
    }

    ImGui::EndMenuBar();
  }

  for (std::size_t i = 0; i < labelTable->numLabels(); ++i)
  {
    char labelIndexBuffer[32];
    snprintf(labelIndexBuffer, 32, "%03zu", i);

    bool labelVisible = labelTable->getVisible(i);
    std::string labelName = labelTable->getName(i);

    // ImGui::ColorEdit represents color as non-pre-multiplied colors
    glm::vec4 labelColor = glm::vec4{labelTable->getColor(i), labelTable->getAlpha(i)} / 255.0f;

    ImGui::PushID(static_cast<int>(i)); /*** PushID i ***/

    if (ImGui::Checkbox("##labelVisible", &labelVisible))
    {
      labelTable->setVisible(i, labelVisible);
      updateLabelColorTableTexture(tableIndex);
    }

    ImGui::SameLine();
    if (ImGui::ColorEdit4(labelIndexBuffer, glm::value_ptr(labelColor), sk_colorEditFlags))
    {
      labelTable->setColor(i, glm::u8vec3{255.0f * labelColor});
      labelTable->setAlpha(i, static_cast<uint8_t>(255.0f * labelColor.a));
      updateLabelColorTableTexture(tableIndex);
    }

    ImGui::SameLine();
    if (ImGui::Button(ICON_FK_HAND_O_UP))
    {
      moveCrosshairsToSegLabelCentroid(i);

      /// @todo Should the views recenter? This done when moving crosshairs to a landmark.

      // With second argument set to true, this function centers all views on the crosshairs.
      // That way, views show the crosshairs even if they were not in the original view bounds.
      //            recenterAllViews( false, true, false );
    }
    if (ImGui::IsItemHovered())
    {
      ImGui::SetTooltip("Move crosshairs to segmentation label centroid");
    }

    ImGui::SameLine();

    // ImGui::PushItemWidth( 175.0f );
    ImGui::PushItemWidth(-1);
    if (ImGui::InputText("##labelName", &labelName))
    {
      labelTable->setName(i, labelName);
    }
    ImGui::PopItemWidth();

    if (scrollToBottomOfLmList)
    {
      if (i == (labelTable->numLabels() - 1))
      {
        ImGui::SetScrollHereY(1.0f);
        scrollToBottomOfLmList = false;
      }
    }

    ImGui::PopID(); /*** PopID i ***/
  }

  ImGui::EndChild();
}

void renderPaletteWindow(
  const char* name,
  bool* showPaletteWindow,
  const std::function<std::size_t(void)>& getNumImageColorMaps,
  const std::function<const ImageColorMap*(std::size_t cmapIndex)>& getImageColorMap,
  const std::function<std::size_t(void)>& getCurrentImageColorMapIndex,
  const std::function<void(std::size_t cmapIndex)>& setCurrentImageColormapIndex,
  const std::function<bool(void)>& getImageColorMapInverted,
  const std::function<bool(void)>& getImageColorMapContinuous,
  const std::function<int(void)>& getImageColorMapLevels,
  const glm::vec3& hsvModFactors,
  const std::function<void(void)>& updateImageUniforms
)
{
  /// @todo model this after the Example: Property editor in ImGui

  static constexpr float sk_labelWidth = 0.25f;
  static constexpr float sk_cmapWidth = 0.75f;

  if (!showPaletteWindow || !*showPaletteWindow)
    return;

  ImGui::SetNextWindowSize(ImVec2(600, 500), ImGuiCond_FirstUseEver);

  ImGui::PushID(name); /*** PushID name ***/

  const bool showWindow = ImGui::Begin(name, showPaletteWindow, ImGuiWindowFlags_NoCollapse);

  if (!showWindow)
  {
    ImGui::PopID(); /*** PopID name ***/
    ImGui::End();
    return;
  }

  std::string infoText("Color maps are ");

  if (getImageColorMapInverted())
  {
    if (!getImageColorMapContinuous())
    {
      infoText += "inverted and quantized into " + std::to_string(getImageColorMapLevels())
                  + " discrete levels for this image component.";
    }
    else
    {
      infoText += "inverted and continnuous for this image component.";
    }
  }
  else
  {
    if (!getImageColorMapContinuous())
    {
      infoText += "quantized into " + std::to_string(getImageColorMapLevels())
                  + " discrete levels for this image component.";
    }
    else
    {
      infoText += "continuous for this image component.";
    }
  }

  ImGui::Text("%s", infoText.c_str());
  ImGui::Spacing();

  const auto& io = ImGui::GetIO();
  const auto& style = ImGui::GetStyle();

  // const float border = style.FramePadding.x;
  const float contentWidth = ImGui::GetContentRegionAvail().x;
  const float height = (io.Fonts->Fonts[0]->FontSize * io.FontGlobalScale) - style.FramePadding.y;
  const ImVec2 buttonSize(sk_cmapWidth * contentWidth, height);

  ImGui::Columns(2, "Colormaps", false);
  ImGui::SetColumnWidth(0, sk_labelWidth * contentWidth);

  for (std::size_t i = 0; i < getNumImageColorMaps(); ++i)
  {
    ImGui::PushID(static_cast<int>(i));
    {
      const ImageColorMap* cmap = getImageColorMap(i);
      if (!cmap)
        continue;

      //                    ImGui::SetCursorPosX( border );
      //                    ImGui::AlignTextToFramePadding();

      if (ImGui::Selectable(
            cmap->name().c_str(),
            (getCurrentImageColorMapIndex() == i),
            ImGuiSelectableFlags_SpanAllColumns
          ))
      {
        setCurrentImageColormapIndex(i);
        updateImageUniforms();
      }

      ImGui::NextColumn();

      const bool doQuantize
        = (!getImageColorMapContinuous() && (ImageColorMap::InterpolationMode::Linear == cmap->interpolationMode()));

      ImGui::paletteButton(
        cmap->name().c_str(),
        cmap->data_RGBA_asVector(),
        getImageColorMapInverted(),
        doQuantize,
        getImageColorMapLevels(),
        hsvModFactors,
        buttonSize
      );

      if (ImGui::IsItemHovered())
      {
        ImGui::SetTooltip("%s", cmap->description().c_str());
      }

      ImGui::NextColumn();
    }
    ImGui::PopID(); // i
  }

  ImGui::End();

  ImGui::PopID(); /*** PopID name ***/
}

void renderLandmarkChildWindow(
  const AppData& appData,
  const ImageTransformations& imageTransformations,
  LandmarkGroup* activeLmGroup,
  const glm::vec3& worldCrosshairsPos,
  const std::function<void(const glm::vec3& worldCrosshairsPos)>& setWorldCrosshairsPos,
  const AllViewsRecenterType& recenterAllViews
)
{
  static const std::string sk_addNew = std::string(ICON_FK_PLUS) + " Add new";
  static const std::string sk_showAll = std::string(ICON_FK_EYE) + " Show all";
  static const std::string sk_hideAll = std::string(ICON_FK_EYE_SLASH) + " Hide all";

  static const ImGuiColorEditFlags sk_colorEditFlags = ImGuiColorEditFlags_NoInputs
                                                       | ImGuiColorEditFlags_PickerHueBar
                                                       | ImGuiColorEditFlags_DisplayRGB
                                                       | ImGuiColorEditFlags_DisplayHSV
                                                       | ImGuiColorEditFlags_DisplayHex
                                                       | ImGuiColorEditFlags_Uint8
                                                       | ImGuiColorEditFlags_InputRGB;

  static const auto sk_hueMinMax = std::make_pair(0.0f, 360.0f);
  static const auto sk_satMinMax = std::make_pair(0.3f, 1.0f);
  static const auto sk_valMinMax = std::make_pair(0.3f, 1.0f);

  const char* coordFormat = appData.guiData().m_coordsPrecisionFormat.c_str();

  std::map<size_t, PointRecord<glm::vec3> >& points = activeLmGroup->getPoints();

  const bool childVisible = ImGui::BeginChild(
    "", ImVec2(375, 300), true, ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_HorizontalScrollbar
  );

  if (!childVisible)
  {
    ImGui::EndChild();
    return;
  }

  bool scrollToBottomOfLmList = false;

  if (ImGui::BeginMenuBar())
  {
    /// @todo Pull this function out of here.
    /// Will need to add concept of "active image or landmarking".
    if (ImGui::MenuItem(sk_addNew.c_str()))
    {
      // Add new landmark at crosshairs position in the correct space
      const glm::mat4 landmark_T_world = (activeLmGroup->getInVoxelSpace())
                                           ? imageTransformations.pixel_T_worldDef()
                                           : imageTransformations.subject_T_worldDef();

      const glm::vec4 lmPos = landmark_T_world * glm::vec4{worldCrosshairsPos, 1.0f};

      PointRecord<glm::vec3> pointRec{glm::vec3{lmPos / lmPos.w}};

      // Assign the new point a random color, seeded by its index
      const size_t newIndex = (activeLmGroup->getPoints().empty()) ? 0u
                                                                   : activeLmGroup->maxIndex() + 1;

      const auto colors = math::generateRandomHsvSamples(
        1, sk_hueMinMax, sk_satMinMax, sk_valMinMax, static_cast<uint32_t>(newIndex)
      );

      if (!colors.empty())
      {
        pointRec.setColor(glm::rgbColor(colors[0]));
      }

      activeLmGroup->addPoint(newIndex, pointRec);

      // Scroll child window to the end of the list of landmarks
      scrollToBottomOfLmList = true;
    }

    if (ImGui::MenuItem(sk_showAll.c_str()))
    {
      for (auto& p : points)
      {
        p.second.setVisibility(true);
      }
    }

    if (ImGui::MenuItem(sk_hideAll.c_str()))
    {
      for (auto& p : points)
      {
        p.second.setVisibility(false);
      }
    }

    ImGui::EndMenuBar();
  }

  char pointIndexBuffer[8];

  for (auto& p : points)
  {
    const size_t pointIndex = p.first;
    auto& point = p.second;

    snprintf(pointIndexBuffer, 8, "%03zu", pointIndex);

    bool pointVisible = point.getVisibility();
    std::string pointName = point.getName();
    glm::vec3 pointColor = point.getColor();
    glm::vec3 pointPos = point.getPosition();

    ImGui::PushID(static_cast<int>(pointIndex)); /*** PushID pointIndex ***/

    if (ImGui::Checkbox(pointIndexBuffer, &pointVisible))
    {
      point.setVisibility(pointVisible);
    }

    if (!activeLmGroup->getColorOverride())
    {
      ImGui::SameLine();
      if (ImGui::ColorEdit3("", glm::value_ptr(pointColor), sk_colorEditFlags))
      {
        point.setColor(pointColor);
      }
    }

    ImGui::SameLine();

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(2.0f, 4.0f));

    if (ImGui::Button(ICON_FK_HAND_O_UP))
    {
      const glm::mat4 world_T_landmark = (activeLmGroup->getInVoxelSpace())
                                           ? imageTransformations.worldDef_T_pixel()
                                           : imageTransformations.worldDef_T_subject();

      const glm::vec4 worldPos = world_T_landmark * glm::vec4{pointPos, 1.0f};
      setWorldCrosshairsPos(glm::vec3{worldPos / worldPos.w});

      // With second argument set to true, this function centers all views on the crosshairs.
      // That way, views show the crosshairs even if they were not in the original view bounds.
      recenterAllViews(false, true, false, false);
    }
    if (ImGui::IsItemHovered())
    {
      ImGui::SetTooltip("Move crosshairs to landmark and center views on landmark");
    }

    ImGui::SameLine();
    if (ImGui::Button(ICON_FK_CROSSHAIRS))
    {
      const glm::mat4 landmark_T_world = (activeLmGroup->getInVoxelSpace())
                                           ? imageTransformations.pixel_T_worldDef()
                                           : imageTransformations.subject_T_worldDef();

      const glm::vec4 lmPos = landmark_T_world * glm::vec4{worldCrosshairsPos, 1.0f};

      point.setPosition(glm::vec3{lmPos / lmPos.w});
    }
    if (ImGui::IsItemHovered())
    {
      ImGui::SetTooltip("Set landmark to the current crosshairs position");
    }

    ImGui::SameLine();
    if (ImGui::Button(ICON_FK_TIMES))
    {
      if (activeLmGroup->removePoint(pointIndex))
      {
        // The point was removed, so skip rendering it
        ImGui::PopID(); // pointIndex
        ImGui::EndChild();
        return;
      }
    }
    if (ImGui::IsItemHovered())
    {
      ImGui::SetTooltip("Delete landmark");
    }

    // ImGuiStyleVar_ItemSpacing
    ImGui::PopStyleVar();

    if (activeLmGroup->getRenderLandmarkNames())
    {
      // Edit the name if they are visible
      ImGui::SameLine();
      ImGui::PushItemWidth(100.0f);
      if (ImGui::InputText("##pointName", &pointName))
      {
        point.setName(pointName);
      }
      ImGui::PopItemWidth();
      if (ImGui::IsItemHovered())
      {
        ImGui::SetTooltip("Landmark name");
      }
    }

    ImGui::SameLine();

    ImGui::PushStyleVar(ImGuiStyleVar_ItemInnerSpacing, ImVec2(1.0f, 4.0f));
    ImGui::PushItemWidth(200.0f);
    if (ImGui::InputFloat3("##pointPos", glm::value_ptr(pointPos), coordFormat, 0))
    {
      point.setPosition(pointPos);
    }

    if (ImGui::IsItemHovered())
    {
      if (activeLmGroup->getInVoxelSpace())
      {
        ImGui::SetTooltip("(x, y, z) voxel position");
      }
      else
      {
        ImGui::SetTooltip("(x, y, z) physical position (mm)");
      }
    }

    ImGui::PopItemWidth();
    ImGui::PopStyleVar(); // ImGuiStyleVar_ItemInnerSpacing

    if (scrollToBottomOfLmList)
    {
      if (pointIndex == (points.size() - 1))
      {
        ImGui::SetScrollHereY(1.0f);
        scrollToBottomOfLmList = false;
      }
    }

    ImGui::PopID(); /*** PopID pointIndex ***/
  }

  ImGui::EndChild();
}
