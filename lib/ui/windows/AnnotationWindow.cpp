#include "ui/windows/AnnotationWindow.h"

#include "ui/Helpers.h"
#include "ui/headers/Headers.h"

#include "logic/app/Data.h"

#include <glm/vec3.hpp>
#include <imgui/imgui.h>

namespace
{
using uuid = uuids::uuid;
}

void renderAnnotationWindow(
  AppData& appData,
  const std::function<void(const uuid& viewUid, const glm::vec3& worldFwdDirection)>& setViewCameraDirection,
  const std::function<void()>& paintActiveSegmentationWithActivePolygon,
  const AllViewsRecenterType& recenterAllViews)
{
  setNextWindowSizeConstraintsToMainViewport(280.0f, 240.0f);
  ImGui::SetNextWindowSize(ImVec2{360.0f, 480.0f}, ImGuiCond_FirstUseEver);
  setNextDockablePanelWindowClass();
  if (ImGui::Begin("Annotations", &(appData.guiData().m_showAnnotationsWindow))) {
    size_t imageIndex = 0;
    const auto activeUid = appData.activeImageUid();

    for (const auto& imageUid : appData.imageUidsOrdered()) {
      const bool isActiveImage = activeUid && (imageUid == *activeUid);
      renderAnnotationsHeader(
        appData,
        imageUid,
        imageIndex++,
        isActiveImage,
        setViewCameraDirection,
        paintActiveSegmentationWithActivePolygon,
        recenterAllViews);
    }
  }

  ImGui::End();
}
