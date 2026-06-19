#pragma once

#include <glm/mat4x4.hpp>
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
 * @param constrainIsotropic If true, use one scale factor for all axes.
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
  bool constrainIsotropic);

} // namespace entropy::app
