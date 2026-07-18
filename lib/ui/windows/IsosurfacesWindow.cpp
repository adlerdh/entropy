#include "ui/windows/IsosurfacesWindow.h"

#include "ui/Helpers.h"
#include "ui/Scaling.h"
#include "ui/headers/IsosurfaceHeader.h"

#include "logic/app/Data.h"

#include <imgui/imgui.h>

#include <future>

namespace
{
using uuid = uuids::uuid;
}

void renderIsosurfacesWindow(
  AppData& appData,
  const std::function<void(const uuid& taskUid, std::future<AsyncTaskDetails> future)>& storeFuture,
  const std::function<void(const uuid& taskUid)>& addTaskToIsosurfaceGpuMeshGenerationQueue)
{
  setNextWindowSizeConstraintsToMainViewport(ui::scaledPixel(300.0f), ui::scaledPixel(240.0f));
  ImGui::SetNextWindowSize(ui::viewportClampedScaledSize(380.0f, 480.0f), ImGuiCond_FirstUseEver);
  setNextDockablePanelWindowClass();
  if (ImGui::Begin("Isosurfaces", &(appData.guiData().m_showIsosurfacesWindow))) {
    std::size_t imageIndex = 0;
    const auto activeUid = appData.activeImageUid();
    const std::size_t visibleImageCount =
      std::min(appData.numImages(), appData.guiData().m_visibleImageCountDuringLoad.value_or(appData.numImages()));

    for (const auto& imageUid : appData.imageUidsOrdered()) {
      if (imageIndex >= visibleImageCount) {
        break;
      }
      const bool isActiveImage = activeUid && (imageUid == *activeUid);
      const bool hasFollowingHeader = imageIndex + 1 < visibleImageCount;

      renderIsosurfacesHeader(
        appData,
        imageUid,
        imageIndex++,
        isActiveImage,
        hasFollowingHeader,
        storeFuture,
        addTaskToIsosurfaceGpuMeshGenerationQueue);
    }
  }
  ImGui::End();
}
