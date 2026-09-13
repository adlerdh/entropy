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
  const std::function<void(void)>& updateImageUniforms)
{
  /// @todo model this after the Example: Property editor in ImGui

  static constexpr float sk_labelWidth = 0.25f;
  static constexpr float sk_cmapWidth = 0.75f;

  if (!showPaletteWindow || !*showPaletteWindow) return;

  ImGui::SetNextWindowSize(ImVec2(600, 500), ImGuiCond_FirstUseEver);
  setNextWindowSizeConstraintsToMainViewport();

  ImGui::PushID(name); /*** PushID name ***/

  const bool showWindow = ImGui::Begin(name, showPaletteWindow, ImGuiWindowFlags_NoCollapse);

  if (!showWindow) {
    ImGui::PopID(); /*** PopID name ***/
    ImGui::End();
    return;
  }

  std::string infoText("Color maps are ");

  if (getImageColorMapInverted()) {
    if (!getImageColorMapContinuous()) {
      infoText += "inverted and quantized into " + std::to_string(getImageColorMapLevels()) +
                  " discrete levels for this image component.";
    }
    else {
      infoText += "inverted and continnuous for this image component.";
    }
  }
  else {
    if (!getImageColorMapContinuous()) {
      infoText +=
        "quantized into " + std::to_string(getImageColorMapLevels()) + " discrete levels for this image component.";
    }
    else {
      infoText += "continuous for this image component.";
    }
  }

  ImGui::Text("%s", infoText.c_str());
  ImGui::Spacing();

  const auto& style = ImGui::GetStyle();

  // const float border = style.FramePadding.x;
  const float contentWidth = ImGui::GetContentRegionAvail().x;
  const float height = ImGui::GetFontSize() - style.FramePadding.y;
  const ImVec2 buttonSize(sk_cmapWidth * contentWidth, height);

  ImGui::Columns(2, "Colormaps", false);
  ImGui::SetColumnWidth(0, sk_labelWidth * contentWidth);

  for (std::size_t i = 0; i < getNumImageColorMaps(); ++i) {
    ImGui::PushID(static_cast<int>(i));
    {
      const ImageColorMap* cmap = getImageColorMap(i);
      if (!cmap) continue;

      //                    ImGui::SetCursorPosX( border );
      //                    ImGui::AlignTextToFramePadding();

      if (ImGui::Selectable(
            cmap->name().c_str(),
            (getCurrentImageColorMapIndex() == i),
            ImGuiSelectableFlags_SpanAllColumns))
      {
        setCurrentImageColormapIndex(i);
        updateImageUniforms();
      }

      ImGui::NextColumn();

      const bool doQuantize =
        (!getImageColorMapContinuous() && (ImageColorMap::InterpolationMode::Linear == cmap->interpolationMode()));

      ImGui::PushID("##paletteButton");
      ImGui::paletteButton(
        cmap->name().c_str(),
        cmap->data_RGBA_asVector(),
        getImageColorMapInverted(),
        doQuantize,
        getImageColorMapLevels(),
        hsvModFactors,
        buttonSize);
      ImGui::PopID();

      if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", cmap->description().c_str());
      }

      ImGui::NextColumn();
    }
    ImGui::PopID(); // i
  }

  ImGui::End();

  ImGui::PopID(); /*** PopID name ***/
}
