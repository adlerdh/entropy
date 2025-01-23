#include "logic/interaction/ViewHit.h"

#include "common/DataHelper.h"
#include "logic/app/Data.h"
#include "logic/camera/CameraHelpers.h"

std::optional<ViewHit> getViewHit(
  AppData& appData, const glm::vec2& windowPos, const std::optional<uuids::uuid>& viewUidForOverride
)
{
  ViewHit hit;

  if (const auto viewUid = appData.windowData().currentViewUidAtCursor(windowPos))
  {
    // Hit a view
    hit.viewUid = *viewUid;
  }
  else if (viewUidForOverride)
  {
    // Did not hit a view, so use the override view
    hit.viewUid = *viewUidForOverride;
  }
  else
  {
    // Did not hit a view and no override provided, so return null
    return std::nullopt;
  }

  hit.view = appData.windowData().getCurrentView(hit.viewUid);

  if (!hit.view)
  {
    // Invalid view
    return std::nullopt;
  }

  // View to use for transformations. Use the override if provided:
  const View* txView = (viewUidForOverride)
                         ? appData.windowData().getCurrentView(*viewUidForOverride)
                         : hit.view;

  if (!txView)
  {
    // Invalid view
    return std::nullopt;
  }

  if (camera::ViewRenderMode::Disabled == hit.view->renderMode())
  {
    return std::nullopt;
  }

  hit.worldFrontAxis = camera::worldDirection(txView->camera(), Directions::View::Front);

  const glm::vec4 winClipPos(
    camera::windowNdc_T_window(appData.windowData().viewport(), windowPos),
    txView->clipPlaneDepth(),
    1.0f
  );

  hit.windowClipPos = glm::vec2{winClipPos};

  glm::vec4 viewClipPos = txView->viewClip_T_windowClip() * winClipPos;
  viewClipPos /= viewClipPos.w;

  hit.viewClipPos = glm::vec2{viewClipPos};

  const glm::mat4 world_T_clip = camera::world_T_clip(txView->camera());

  // Apply view's offset from crosshairs in order to calculate the view plane position:
  const float offsetDist
    = data::computeViewOffsetDistance(appData, txView->offsetSetting(), hit.worldFrontAxis);

  const glm::vec4 offset{offsetDist * hit.worldFrontAxis, 0.0f};

  glm::vec4 worldPos = world_T_clip * viewClipPos;
  worldPos = worldPos / worldPos.w;

  hit.worldPos_offsetApplied = worldPos;

  // Note: This undoes the offset, so that lightbox views don't shift.
  hit.worldPos = worldPos - offset;
  hit.worldPos = glm::vec4{data::snapWorldPointToImageVoxels(appData, glm::vec3{hit.worldPos}), 1.0f};

  return hit;
}
