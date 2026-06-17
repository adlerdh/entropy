#pragma once

#include "common/Viewport.h"

#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec4.hpp>
#include <uuid.h>

#include <optional>
#include <span>

namespace viewer
{

/**
 * @brief Coordinate positions for a hit in one frame.
 */
struct FrameHitGeometry
{
  glm::vec2 m_windowClipPos{0.0f}; //!< Hit position in enclosing-window clip coordinates.
  glm::vec2 m_viewClipPos{0.0f};   //!< Hit position in frame clip coordinates.
  glm::vec4 m_worldPos{0.0f};      //!< Hit position in world coordinates.
};

/**
 * @brief Compute frame hit coordinates from a window position.
 * @param windowViewport Enclosing window viewport.
 * @param windowPos Position in window coordinates, with origin at bottom-left.
 * @param viewClip_T_windowClip Transform from window clip coordinates to frame clip coordinates.
 * @param world_T_viewClip Transform from frame clip coordinates to world coordinates.
 * @param clipPlaneDepth Clip-space depth to use for the hit plane.
 * @return Hit coordinates in window clip, frame clip, and world space.
 */
FrameHitGeometry computeFrameHitGeometry(
  const Viewport& windowViewport,
  const glm::vec2& windowPos,
  const glm::mat4& viewClip_T_windowClip,
  const glm::mat4& world_T_viewClip,
  float clipPlaneDepth);

/**
 * @brief Candidate frame for viewport-based selection.
 */
struct FrameHitTarget
{
  uuids::uuid m_uid;                    //!< Frame UID.
  glm::vec4 m_windowClipViewport{0.0f}; //!< Bounds in enclosing-window clip coordinates.
};

/**
 * @brief Resolved view hit and transform targets.
 */
struct FrameHitSelection
{
  uuids::uuid m_hitFrameUid;       //!< Frame that receives the hit.
  uuids::uuid m_transformFrameUid; //!< Frame used for coordinate transforms.
};

/**
 * @brief Find the first frame containing a window position.
 * @param windowViewport Enclosing window viewport.
 * @param windowPos Position in window coordinates, with origin at bottom-left.
 * @param frames Frames in hit-test order.
 * @return UID of the first containing frame, or `std::nullopt`.
 */
std::optional<uuids::uuid> findFrameAtWindowPosition(
  const Viewport& windowViewport,
  const glm::vec2& windowPos,
  std::span<const FrameHitTarget> frames);

/**
 * @brief Find the frame with the largest viewport area.
 * @param frames Frames in priority order for equal areas.
 * @return UID of the largest frame, or `std::nullopt` when no frames are provided.
 */
std::optional<uuids::uuid> findLargestFrameByArea(std::span<const FrameHitTarget> frames);

/**
 * @brief Resolve hit and transform frame UIDs from a direct hit and optional override.
 * @param frameAtPosition Frame under the cursor, when any.
 * @param overrideFrameUid Frame to use when no direct hit exists, and always for transforms when provided.
 * @return Hit and transform UIDs, or `std::nullopt` when neither input names a frame.
 */
std::optional<FrameHitSelection> resolveFrameHitSelection(
  std::optional<uuids::uuid> frameAtPosition,
  std::optional<uuids::uuid> overrideFrameUid);

} // namespace viewer
