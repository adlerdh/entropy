#include "ui/widgets/Widgets.h"
#include "ui/widgets/ActiveImageSelectionModel.h"
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

void renderActiveImageSelectionCombo(
  size_t numImages,
  const std::function<std::pair<std::string, std::string>(std::size_t index)>& getImageDisplayAndFileName,
  const std::function<std::size_t(void)>& getActiveImageIndex,
  const std::function<void(std::size_t)>& setActiveImageIndex,
  bool showText)
{
  const std::size_t activeIndex = getActiveImageIndex();

  switch (entropy::ui::active_image_selection::selectionState(numImages, activeIndex)) {
    case entropy::ui::active_image_selection::SelectionState::Empty:
      return;
    case entropy::ui::active_image_selection::SelectionState::Invalid:
      spdlog::error("Invalid active image index");
      return;
    case entropy::ui::active_image_selection::SelectionState::Valid:
      break;
  }

  const std::string nameString = (showText) ? "Active image###imageSelectionCombo" : "###imageSelectionCombo";

  //    ImGui::Text( "Active image:" );

  //    ImGui::PushItemWidth( -1 );
  if (ImGui::BeginCombo(nameString.c_str(), getImageDisplayAndFileName(activeIndex).first.c_str())) {
    for (std::size_t i = 0; i < numImages; ++i) {
      const auto displayAndFileName = getImageDisplayAndFileName(i);
      const bool isSelected = (i == activeIndex);

      ImGui::PushID(static_cast<int>(i)); // needed in case two images have the same display name
      if (ImGui::Selectable(displayAndFileName.first.c_str(), isSelected)) {
        setActiveImageIndex(i);
      }
      ImGui::PopID();

      if (isSelected) {
        ImGui::SetItemDefaultFocus();
      }
    }

    ImGui::EndCombo();
  }
  //    ImGui::PopItemWidth();

  ImGui::SameLine();
  helpMarker("Select the image that is being actively transformed, adjusted, or segmented");
}
