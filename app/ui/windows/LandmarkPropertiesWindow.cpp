#include "ui/windows/LandmarkPropertiesWindow.h"

#include "ui/headers/Headers.h"

#include "logic/app/Data.h"

#include <imgui/imgui.h>

namespace
{
using uuid = uuids::uuid;
}

void renderLandmarkPropertiesWindow(
  AppData& appData,
  const AllViewsRecenterType& recenterAllViewsOnCurrentCrosshairsPosition)
{
  if (ImGui::Begin("Landmarks", &(appData.guiData().m_showLandmarksWindow), ImGuiWindowFlags_AlwaysAutoResize)) {
    size_t imageIndex = 0;
    const auto activeUid = appData.activeImageUid();

    for (const auto& imageUid : appData.imageUidsOrdered()) {
      const bool isActiveImage = activeUid && (imageUid == *activeUid);

      renderLandmarkGroupHeader(
        appData,
        imageUid,
        imageIndex++,
        isActiveImage,
        recenterAllViewsOnCurrentCrosshairsPosition);
    }
  }

  ImGui::End();
}
