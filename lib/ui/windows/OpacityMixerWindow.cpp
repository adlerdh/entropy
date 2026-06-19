#include "ui/windows/OpacityMixerWindow.h"

#include "image/Image.h"
#include "logic/app/Data.h"
#include "rendering/RenderData.h"
#include "ui/Helpers.h"
#include "ui/windows/OpacityMixerModel.h"

#include <imgui/imgui.h>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/color_space.hpp>

#include <string>

void renderOpacityBlenderWindow(
  AppData& appData,
  const std::function<void(const uuids::uuid& imageUid)>& updateImageUniforms)
{
  RenderData& renderData = appData.renderData();

  static const char* windowName = "Image Opacity Mixer";

  if (!appData.guiData().m_showOpacityBlenderWindow) {
    return;
  }

  setNextWindowSizeConstraintsToMainViewport(280.0f, 180.0f);
  ImGui::SetNextWindowSize(ImVec2{360.0f, 260.0f}, ImGuiCond_FirstUseEver);
  setNextDockablePanelWindowClass();
  const bool showWindow =
    ImGui::Begin(windowName, &(appData.guiData().m_showOpacityBlenderWindow), ImGuiWindowFlags_NoCollapse);

  if (!showWindow) {
    ImGui::End();
    return;
  }

  int imageIndex = 0;

  for (const auto& imageUid : appData.imageUidsOrdered()) {
    Image* image = appData.image(imageUid);
    if (!image) {
      continue;
    }

    ImageSettings& imgSettings = image->settings();
    const ImageHeader& imgHeader = image->header();

    const glm::vec3 borderColorHsv = glm::hsvColor(imgSettings.borderColor());

    const float hue = borderColorHsv[0];
    const float sat = borderColorHsv[1];
    const float val = borderColorHsv[2];

    const glm::vec3 frameBgColor = glm::rgbColor(glm::vec3{hue, 0.5f * sat, 0.5f * val});
    const glm::vec3 frameBgActiveColor = glm::rgbColor(glm::vec3{hue, 0.7f * sat, 0.5f * val});
    const glm::vec3 frameBgHoveredColor = glm::rgbColor(glm::vec3{hue, 0.6f * sat, 0.5f * val});
    const glm::vec3 sliderGrabColor = glm::rgbColor(glm::vec3{hue, sat, val});

    ImGui::PushID(imageIndex);

    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(frameBgColor.r, frameBgColor.g, frameBgColor.b, 1.0f));
    ImGui::PushStyleColor(
      ImGuiCol_FrameBgActive,
      ImVec4(frameBgActiveColor.r, frameBgActiveColor.g, frameBgActiveColor.b, 1.0f));
    ImGui::PushStyleColor(
      ImGuiCol_FrameBgHovered,
      ImVec4(frameBgHoveredColor.r, frameBgHoveredColor.g, frameBgHoveredColor.b, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_SliderGrab, ImVec4(sliderGrabColor.r, sliderGrabColor.g, sliderGrabColor.b, 1.0f));

    const std::string name = imgSettings.displayName() + "##" + std::to_string(imageIndex);

    if (imgSettings.displayImageAsColor()) {
      double opacity = imgSettings.globalOpacity();
      if (mySliderF64(name.c_str(), &opacity, 0.0, 1.0) && !renderData.m_opacityMixMode) {
        imgSettings.setGlobalOpacity(opacity);
        updateImageUniforms(imageUid);
      }
    }
    else {
      double opacity = imgSettings.opacity();
      if (mySliderF64(name.c_str(), &opacity, 0.0, 1.0) && !renderData.m_opacityMixMode) {
        imgSettings.setOpacity(opacity);
        updateImageUniforms(imageUid);
      }
    }

    if (ImGui::IsItemActive() || ImGui::IsItemHovered()) {
      ImGui::SetTooltip("%s", imgHeader.fileName().c_str());
    }

    ImGui::PopStyleColor(4);
    ImGui::PopID();
  }

  static double mix = 0.0;

  if (appData.numImages() > 1) {
    ImGui::Checkbox("Comparison blender", &(renderData.m_opacityMixMode));
    ImGui::SameLine();
    helpMarker("Use a single slider to blend across all adjacent image layers");
  }
  else {
    renderData.m_opacityMixMode = false;
  }

  if (renderData.m_opacityMixMode) {
    mySliderF64("Blend", &mix, 0.0, static_cast<double>(appData.numImages() - 1));

    for (std::size_t i = 0; i < appData.numImages(); ++i) {
      const auto imgUid = appData.imageUid(i);
      if (!imgUid) {
        continue;
      }

      Image* img = appData.image(*imgUid);
      if (!img) {
        continue;
      }

      const double op = entropy::ui::opacity_mixer::blendedOpacity(i, mix);

      if (img->settings().displayImageAsColor()) {
        img->settings().setGlobalOpacity(op);
      }
      else {
        img->settings().setOpacity(op);
      }

      updateImageUniforms(*imgUid);
    }
  }

  ImGui::End();
}
