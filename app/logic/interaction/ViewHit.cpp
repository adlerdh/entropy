#include "logic/interaction/ViewHit.h"

#include "logic/app/DataHelper.h"
#include "logic/app/Data.h"
#include "logic/camera/CameraHelpers.h"
#include "viewer/FrameHitGeometry.h"

std::optional<ViewHit>
getViewHit(AppData& appData, const glm::vec2& windowPos, const std::optional<uuids::uuid>& viewUidForOverride)
{
  ViewHit hit;

  const auto selection =
    viewer::resolveFrameHitSelection(appData.windowData().currentViewUidAtCursor(windowPos), viewUidForOverride);
  if (!selection) {
    return std::nullopt;
  }

  hit.viewUid = selection->m_hitFrameUid;
  hit.view = appData.windowData().getCurrentView(hit.viewUid);
  if (!hit.view) {
    return std::nullopt; // Invalid view
  }

  const View* txView = appData.windowData().getCurrentView(selection->m_transformFrameUid);
  if (!txView) {
    return std::nullopt; // Invalid view
  }

  if (ViewRenderMode::Disabled == hit.view->renderMode()) {
    return std::nullopt;
  }

  hit.worldFrontAxis = helper::worldDirection(txView->camera(), Directions::View::Front);

  const viewer::FrameHitGeometry hitGeometry = viewer::computeFrameHitGeometry(
    appData.windowData().viewport(),
    windowPos,
    txView->viewClip_T_windowClip(),
    helper::world_T_clip(txView->camera()),
    txView->clipPlaneDepth());
  hit.windowClipPos = hitGeometry.m_windowClipPos;
  hit.viewClipPos = hitGeometry.m_viewClipPos;

  // Apply view's offset from crosshairs in order to calculate the view plane position:
  const float offsetDist = data::computeViewOffsetDistance(appData, txView->offsetSetting(), hit.worldFrontAxis);
  const glm::vec4 offset{offsetDist * hit.worldFrontAxis, 0.0f};

  glm::vec4 worldPos = hitGeometry.m_worldPos;
  hit.worldPos_offsetApplied = worldPos;

  // Note: This undoes the offset, so that lightbox views don't shift.
  hit.worldPos = worldPos - offset;
  hit.worldPos = glm::vec4{data::snapWorldPointToImageVoxels(appData, glm::vec3{hit.worldPos}), 1.0f};
  return hit;
}
