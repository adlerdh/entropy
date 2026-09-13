#include "viewer/FrameHitGeometry.h"

namespace viewer
{
namespace
{

glm::vec2 windowClip_T_window(const Viewport& viewport, const glm::vec2& windowPos)
{
  return glm::vec2{
    -1.0f + (2.0f * (windowPos.x - viewport.left()) / viewport.width()),
    -1.0f + (2.0f * (windowPos.y - viewport.bottom()) / viewport.height())};
}

} // namespace

FrameHitGeometry computeFrameHitGeometry(
  const Viewport& windowViewport,
  const glm::vec2& windowPos,
  const glm::mat4& viewClip_T_windowClip,
  const glm::mat4& world_T_viewClip,
  float clipPlaneDepth)
{
  const glm::vec4 windowClipPos{windowClip_T_window(windowViewport, windowPos), clipPlaneDepth, 1.0f};

  glm::vec4 viewClipPos = viewClip_T_windowClip * windowClipPos;
  viewClipPos /= viewClipPos.w;

  glm::vec4 worldPos = world_T_viewClip * viewClipPos;
  worldPos /= worldPos.w;

  return FrameHitGeometry{
    .m_windowClipPos = glm::vec2{windowClipPos},
    .m_viewClipPos = glm::vec2{viewClipPos},
    .m_worldPos = worldPos};
}

std::optional<uuids::uuid> findFrameAtWindowPosition(
  const Viewport& windowViewport,
  const glm::vec2& windowPos,
  std::span<const FrameHitTarget> frames)
{
  const glm::vec2 windowClipPos = windowClip_T_window(windowViewport, windowPos);

  for (const auto& frame : frames) {
    const glm::vec4& viewport = frame.m_windowClipViewport;

    if (
      (viewport.x <= windowClipPos.x) && (windowClipPos.x < viewport.x + viewport.z) &&
      (viewport.y <= windowClipPos.y) && (windowClipPos.y < viewport.y + viewport.w))
    {
      return frame.m_uid;
    }
  }

  return std::nullopt;
}

std::optional<uuids::uuid> findLargestFrameByArea(std::span<const FrameHitTarget> frames)
{
  if (frames.empty()) {
    return std::nullopt;
  }

  const FrameHitTarget* largest = &frames.front();
  float largestArea = largest->m_windowClipViewport.z * largest->m_windowClipViewport.w;

  for (const auto& frame : frames.subspan(1)) {
    const float area = frame.m_windowClipViewport.z * frame.m_windowClipViewport.w;
    if (area > largestArea) {
      largest = &frame;
      largestArea = area;
    }
  }

  return largest->m_uid;
}

std::optional<FrameHitSelection> resolveFrameHitSelection(
  std::optional<uuids::uuid> frameAtPosition,
  std::optional<uuids::uuid> overrideFrameUid)
{
  if (frameAtPosition) {
    return FrameHitSelection{
      .m_hitFrameUid = *frameAtPosition,
      .m_transformFrameUid = overrideFrameUid.value_or(*frameAtPosition)};
  }

  if (overrideFrameUid) {
    return FrameHitSelection{.m_hitFrameUid = *overrideFrameUid, .m_transformFrameUid = *overrideFrameUid};
  }

  return std::nullopt;
}

} // namespace viewer
