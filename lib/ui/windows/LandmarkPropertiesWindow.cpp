#include "ui/windows/LandmarkPropertiesWindow.h"

#include "ui/Helpers.h"
#include "ui/Scaling.h"
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
  setNextWindowSizeConstraintsToMainViewport(ui::scaledPixel(280.0f), ui::scaledPixel(240.0f));
  ImGui::SetNextWindowSize(ui::viewportClampedScaledSize(360.0f, 480.0f), ImGuiCond_FirstUseEver);
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
