#include "ui/windows/LandmarkPropertiesWindow.h"

#include "ui/Helpers.h"
#include "ui/headers/Headers.h"

#include "logic/app/Data.h"

#include <imgui/imgui.h>

#include <algorithm>

namespace
{
using uuid = uuids::uuid;
}

void renderLandmarkPropertiesWindow(
  AppData& appData,
  const AllViewsRecenterType& recenterAllViewsOnCurrentCrosshairsPosition)
{
  setNextWindowSizeConstraintsToMainViewport(280.0f, 240.0f);
  ImGui::SetNextWindowSize(ImVec2{360.0f, 480.0f}, ImGuiCond_FirstUseEver);
  setNextDockablePanelWindowClass();
  if (ImGui::Begin("Landmarks", &(appData.guiData().m_showLandmarksWindow))) {
    size_t imageIndex = 0;
    const auto activeUid = appData.activeImageUid();
    const std::size_t visibleImageCount =
      std::min(appData.numImages(), appData.guiData().m_visibleImageCountDuringLoad.value_or(appData.numImages()));

    for (const auto& imageUid : appData.imageUidsOrdered()) {
      if (imageIndex >= visibleImageCount) {
        break;
      }
      const bool isActiveImage = activeUid && (imageUid == *activeUid);
      const bool hasFollowingHeader = imageIndex + 1 < visibleImageCount;

      renderLandmarkGroupHeader(
        appData,
        imageUid,
        imageIndex++,
        isActiveImage,
        hasFollowingHeader,
        recenterAllViewsOnCurrentCrosshairsPosition);
    }
  }

  ImGui::End();
}
