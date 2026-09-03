#pragma once

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include <optional>

class Camera;

namespace camera2d
{

/**
 * @brief Convert an incremental pointer drag into a positive, reversible zoom factor.
 * @param windowClipDeltaY Pointer movement in Window Clip space
 * @return Multiplicative zoom factor, or 1 for non-finite input
 */
float dragZoomFactor(float windowClipDeltaY) noexcept;

/**
 * @brief Convert an incremental wheel or trackpad delta into a positive, reversible zoom factor.
 * @param scrollDeltaY Vertical scroll delta
 * @return Multiplicative zoom factor, or 1 for non-finite input
 */
float scrollZoomFactor(float scrollDeltaY) noexcept;

/**
 * @brief Compute a 2D interaction pivot in camera NDC.
 * @param camera Camera used to project an optional World-space pivot
 * @param worldPivot World-space pivot, or no value to use the center of the view
 * @return Finite NDC pivot, or no value when the World-space pivot cannot be projected
 */
std::optional<glm::vec2> interactionPivotNdc(const Camera& camera, const std::optional<glm::vec3>& worldPivot);

/**
 * @brief Controller for one orthographic slice camera.
 *
 * The controller validates interaction input and exposes actual World-space
 * translation and rotation deltas so synchronized cameras can receive the same
 * operation without repeating source-view coordinate calculations.
 */
class Controller
{
public:
  explicit Controller(Camera& camera);

  /**
   * @brief Pan while keeping a World-space anchor under the pointer.
   * @return Actual World-space camera-origin translation, or no value for a no-op
   */
  std::optional<glm::vec3> pan(const glm::vec2& ndcOldPos, const glm::vec2& ndcNewPos, const glm::vec3& worldAnchor);

  /** @brief Apply an exact World-space translation, typically from a synchronized camera. */
  bool translateWorld(const glm::vec3& worldTranslation);

  /**
   * @brief Rotate from two pointer positions about an NDC pivot.
   * @return Applied signed angle in radians, or no value for a no-op
   */
  std::optional<float>
  rotateFromPointer(const glm::vec2& ndcOldPos, const glm::vec2& ndcNewPos, const glm::vec2& ndcPivot);

  /** @brief Apply an exact in-plane rotation about an NDC pivot. */
  bool rotate(float angleRadians, const glm::vec2& ndcPivot);

  /** @brief Apply a multiplicative zoom about an NDC pivot. */
  bool zoom(float factor, const glm::vec2& ndcPivot);

private:
  Camera& m_camera;
};

} // namespace camera2d
