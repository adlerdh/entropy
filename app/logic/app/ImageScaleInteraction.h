#pragma once

#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include <optional>

namespace entropy::app
{

/**
 * @brief Result of one incremental manual image scaling drag.
 */
struct ImageScaleUpdate
{
  glm::vec3 m_scale{1.0f};       //!< New absolute manual scale.
  glm::vec3 m_translation{0.0f}; //!< New manual translation preserving the scale center.
};

/**
 * @brief Constraint applied while scaling an image in the active view plane.
 */
enum class ImageScaleConstraint
{
  Free,           //!< Scale independently along both visible image axes.
  Isotropic,      //!< Use one scale factor for all image axes.
  ViewHorizontal, //!< Scale only along the image axis closest to view-right.
  ViewVertical    //!< Scale only along the image axis closest to view-up.
};

/**
 * @brief Distance in view clip space between the scale center and pointer.
 * @param pointerViewClip Pointer location in view clip coordinates.
 * @param centerViewClip Scale center in view clip coordinates.
 * @return Euclidean distance in view clip space.
 */
float imageScaleViewClipDistance(const glm::vec2& pointerViewClip, const glm::vec2& centerViewClip);

/**
 * @brief Return true when a scale drag starts inside the scale dead-zone.
 * @param startViewClip Drag start location in view clip coordinates.
 * @param centerViewClip Scale center in view clip coordinates.
 * @param radiusViewClip Dead-zone radius in view clip units.
 * @return True when the start location is inside the dead-zone.
 */
bool imageScaleStartsInDeadZone(const glm::vec2& startViewClip, const glm::vec2& centerViewClip, float radiusViewClip);

/**
 * @brief Return true when scaling should wait for a near-center drag to leave the dead-zone.
 * @param startViewClip Drag start location in view clip coordinates.
 * @param currentViewClip Current pointer location in view clip coordinates.
 * @param centerViewClip Scale center in view clip coordinates.
 * @param radiusViewClip Dead-zone radius in view clip units.
 * @return True when the drag began inside the dead-zone and has not left it yet.
 */
bool imageScaleDragShouldWaitForDeadZone(
  const glm::vec2& startViewClip,
  const glm::vec2& currentViewClip,
  const glm::vec2& centerViewClip,
  float radiusViewClip);

/**
 * @brief Choose a view-axis scale constraint once one drag axis clearly dominates.
 * @param dragDeltaViewClip Pointer drag from the scale start in view clip coordinates.
 * @param minDominanceRatio Minimum larger-axis/smaller-axis ratio needed to choose an axis.
 * @return ViewHorizontal, ViewVertical, or std::nullopt when the drag is ambiguous.
 */
std::optional<ImageScaleConstraint> imageScaleViewAxisConstraintFromDrag(
  const glm::vec2& dragDeltaViewClip,
  float minDominanceRatio);

/**
 * @brief Compute an incremental image scale that keeps the crosshair-centered image point under the pointer.
 *
 * @param worldDef_T_affine Current manual transform from affine image space to world space.
 * @param currentScale Current manual scale component.
 * @param fixedWorldCenter Scale center in world space.
 * @param previousWorldPointer World position under the previous pointer position.
 * @param currentWorldPointer World position under the current pointer position.
 * @param viewRight World-space right axis of the active view.
 * @param viewUp World-space up axis of the active view.
 * @param viewFront World-space front axis of the active view.
 * @param constraint Scale constraint applied to the drag.
 * @return New manual scale and translation, or std::nullopt when the drag cannot define a stable scale.
 */
std::optional<ImageScaleUpdate> computeImageScaleUpdate(
  const glm::mat4& worldDef_T_affine,
  const glm::vec3& currentScale,
  const glm::vec3& fixedWorldCenter,
  const glm::vec3& previousWorldPointer,
  const glm::vec3& currentWorldPointer,
  const glm::vec3& viewRight,
  const glm::vec3& viewUp,
  const glm::vec3& viewFront,
  ImageScaleConstraint constraint);

} // namespace entropy::app
