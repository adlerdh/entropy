#include "ui/windows/SegmentationPropertiesWindow.h"

#include "ui/Helpers.h"
#include "ui/headers/Headers.h"

#include "image/Image.h"

#include "logic/app/Data.h"

#include <imgui/imgui.h>

#include <filesystem>
#include <optional>
#include <string>

namespace fs = std::filesystem;

namespace
{
using uuid = uuids::uuid;
}

void renderSegmentationPropertiesWindow(
  AppData& appData,
  const std::function<ParcellationLabelTable*(std::size_t tableIndex)>& getLabelTable,
  const std::function<void(const uuid& imageUid)>& updateImageUniforms,
  const std::function<void(std::size_t labelColorTableIndex)>& updateLabelColorTableTexture,
  const std::function<void(const uuid& imageUid, size_t labelIndex)>& moveCrosshairsToSegLabelCentroid,
  const std::function<std::optional<uuid>(const uuid& matchingImageUid, const std::string& segDisplayName)>&
    createBlankSeg,
  const std::function<void(const uuid& imageUid, const fs::path& fileName)>& addSegmentationFile,
  const std::function<bool(const uuid& segUid)>& clearSeg,
  const std::function<bool(const uuid& segUid)>& removeSeg,
  const AllViewsRecenterType& recenterAllViews)
{
  static bool firstRun = false;

  ImGuiWindowFlags flags = firstRun ? ImGuiWindowFlags_AlwaysAutoResize : ImGuiWindowFlags_None;

  setNextWindowSizeConstraintsToMainViewport();
  if (ImGui::Begin("Segmentations##Segmentations", &(appData.guiData().m_showSegmentationsWindow), flags)) {
    firstRun = false;

    size_t imageIndex = 0;
    const auto activeUid = appData.activeImageUid();

    for (const auto& imageUid : appData.imageUidsOrdered()) {
      if (Image* image = appData.image(imageUid)) {
        const bool isActiveImage = activeUid && (imageUid == *activeUid);

        renderSegmentationHeader(
          appData,
          imageUid,
          imageIndex++,
          image,
          isActiveImage,
          [&imageUid, updateImageUniforms]() { updateImageUniforms(imageUid); },
          getLabelTable,
          updateLabelColorTableTexture,
          [&imageUid, moveCrosshairsToSegLabelCentroid](std::size_t labelIndex) {
            moveCrosshairsToSegLabelCentroid(imageUid, labelIndex);
          },
          createBlankSeg,
          addSegmentationFile,
          clearSeg,
          removeSeg,
          recenterAllViews);
      }
    }
  }

  ImGui::End();
}
