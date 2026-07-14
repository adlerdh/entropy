#include "ui/windows/ImagePropertiesWindow.h"

#include "ui/Helpers.h"
#include "ui/headers/Headers.h"
#include "ui/widgets/Widgets.h"

#include "image/Image.h"

#include "logic/app/Data.h"
#include "logic/app/StackTrace.h"

#include <IconsForkAwesome.h>
#include <imgui/imgui.h>

#include <spdlog/spdlog.h>

#include <algorithm>
#include <string>

namespace
{
using uuid = uuids::uuid;
}

void renderImagePropertiesWindow(
  AppData& appData,
  size_t numImages,
  const std::function<std::pair<std::string, std::string>(std::size_t index)>& getImageDisplayAndFileName,
  const std::function<size_t(void)>& getActiveImageIndex,
  const std::function<void(std::size_t)>& setActiveImageIndex,
  const std::function<size_t(void)>& getNumImageColorMaps,
  const std::function<ImageColorMap*(std::size_t cmapIndex)>& getImageColorMap,
  const std::function<bool(const uuid& imageUid)>& moveImageBackward,
  const std::function<bool(const uuid& imageUid)>& moveImageForward,
  const std::function<bool(const uuid& imageUid)>& moveImageToBack,
  const std::function<bool(const uuid& imageUid)>& moveImageToFront,
  const std::function<void(void)>& updateAllImageUniforms,
  const std::function<void(const uuid& imageUid)>& updateImageUniforms,
  const std::function<void(const uuid& imageUid)>& updateImageInterpolationMode,
  const std::function<void(std::size_t cmapIndex)>& updateImageColorMapInterpolationMode,
  const std::function<std::optional<uuid>(const std::filesystem::path& fileName)>& loadWarpField,
  const std::function<void(
    const uuid& imageUid,
    const std::filesystem::path& fileName,
    bool forwardWarp,
    std::optional<uuid> inverseWarpReferenceImageUid)>& loadAndAssignWarpField,
  const std::function<void(
    const uuid& imageUid,
    const uuid& sourceWarpUid,
    ComputedWarpDirection direction,
    const WarpInversionOptions& options)>& requestWarpInversion,
  const std::function<bool(const uuid& imageUid, bool locked)>& setLockManualImageTransformation,
  const std::function<void(const uuid& imageUid, ComponentProjectionMode mode)>& requestComponentProjectionImage,
  const std::function<void(const uuid& imageUid)>& requestSetReferenceImage,
  const std::function<void(const uuid& imageUid)>& requestRemoveImage,
  const AllViewsRecenterType& recenterAllViews)
{
  static const std::string kShowOpacityMixerButton = std::string(ICON_FK_SLIDERS) + "##showOpacityMixer";

  setNextWindowSizeConstraintsToMainViewport(340.0f, 280.0f);
  ImGui::SetNextWindowSize(ImVec2{460.0f, 620.0f}, ImGuiCond_FirstUseEver);
  setNextDockablePanelWindowClass();
  if (ImGui::Begin("Images##Images", &(appData.guiData().m_showImagePropertiesWindow))) {
    const bool showOpacityMixer = appData.guiData().m_showOpacityBlenderWindow;
    if (showOpacityMixer) {
      ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
    }
    if (ImGui::Button(kShowOpacityMixerButton.c_str())) {
      appData.guiData().m_showOpacityBlenderWindow = !appData.guiData().m_showOpacityBlenderWindow;
    }
    if (showOpacityMixer) {
      ImGui::PopStyleColor();
    }
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("%s", "Show opacity mixer");
    }

    ImGui::SameLine();
    renderActiveImageSelectionCombo(numImages, getImageDisplayAndFileName, getActiveImageIndex, setActiveImageIndex);

    ImGui::Separator();

    size_t imageIndex = 0;
    const auto activeUid = appData.activeImageUid();
    const std::size_t visibleImageCount =
      std::min(numImages, appData.guiData().m_visibleImageCountDuringLoad.value_or(numImages));

    for (const auto& imageUid : appData.imageUidsOrdered()) {
      if (imageIndex >= visibleImageCount) {
        break;
      }
      if (Image* image = appData.image(imageUid)) {
        const bool isActiveImage = activeUid && (imageUid == *activeUid);

        try {
          renderImageHeader(
            appData,
            appData.guiData(),
            imageUid,
            imageIndex++,
            image,
            isActiveImage,
            appData.numImages(),
            updateAllImageUniforms,
            [&imageUid, updateImageUniforms]() { updateImageUniforms(imageUid); },
            [&imageUid, updateImageInterpolationMode]() { updateImageInterpolationMode(imageUid); },
            [updateImageColorMapInterpolationMode](std::size_t cmapIndex) {
              updateImageColorMapInterpolationMode(cmapIndex);
            },
            getNumImageColorMaps,
            getImageColorMap,
            loadWarpField,
            loadAndAssignWarpField,
            requestWarpInversion,
            moveImageBackward,
            moveImageForward,
            moveImageToBack,
            moveImageToFront,
            setLockManualImageTransformation,
            requestComponentProjectionImage,
            requestSetReferenceImage,
            requestRemoveImage,
            recenterAllViews);
        }
        catch (const std::exception& e) {
          spdlog::error(
            "Exception while rendering image header for image {} ('{}'): {}\n{}",
            imageUid,
            image->settings().displayName(),
            e.what(),
            stack_trace::current(1));
          throw;
        }
      }
    }

    //        const double frameRate = static_cast<double>( ImGui::GetIO().Framerate );
    //        ImGui::Text( "Rendering: %.3f ms/frame (%.1f FPS)", 1000.0 / frameRate, frameRate );
  }

  ImGui::End();
}
