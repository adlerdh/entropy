#include "ui/windows/IsosurfacesWindow.h"

#include "ui/Helpers.h"
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
  std::function<void(const uuid& taskUid, std::future<AsyncTaskDetails> future)> storeFuture,
  std::function<void(const uuid& taskUid)> addTaskToIsosurfaceGpuMeshGenerationQueue)
{
  setNextWindowSizeConstraintsToMainViewport(300.0f, 240.0f);
  ImGui::SetNextWindowSize(ImVec2{380.0f, 480.0f}, ImGuiCond_FirstUseEver);
  setNextDockablePanelWindowClass();
  if (ImGui::Begin("Isosurfaces", &(appData.guiData().m_showIsosurfacesWindow))) {
    std::size_t imageIndex = 0;
    const auto activeUid = appData.activeImageUid();

    for (const auto& imageUid : appData.imageUidsOrdered()) {
      const bool isActiveImage = activeUid && (imageUid == *activeUid);

      renderIsosurfacesHeader(
        appData,
        imageUid,
        imageIndex++,
        isActiveImage,
        storeFuture,
        addTaskToIsosurfaceGpuMeshGenerationQueue);
    }
  }
  ImGui::End();
}
