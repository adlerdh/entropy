#pragma once

#include "rendering/utility/math/SliceIntersectorTypes.h"

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <optional>
#include <utility>

/**
 * @brief Computes the polygon where a model-space box is cut by an interactively positioned slice plane.
 *
 * The intersector stores the rules used to position and orient the plane. `computePlaneIntersections()` receives the
 * current model transforms and box corners, updates the active model-space plane equation, and returns the ordered
 * intersection vertices when the plane cuts through the box.
 */
class SliceIntersector
{
public:
  static constexpr int s_numIntersections = 6; //!< Maximum number of edges a plane can intersect on a box

  /// Maximum number of vertices stored by the historical intersection path, including a repeated closing vertex.
  static constexpr int s_numVertices = 7;

  explicit SliceIntersector();

  SliceIntersector(const SliceIntersector&) = default;
  SliceIntersector(SliceIntersector&&) = default;

  SliceIntersector& operator=(const SliceIntersector&) = default;
  SliceIntersector& operator=(SliceIntersector&&) = default;

  ~SliceIntersector() = default;

  /**
   * @brief Select how the slice plane position is derived.
   *
   * @param method Positioning mode.
   * @param positionOrCameraOffset Optional mode-specific vector. For `UserDefined`, this is the model-space point on
   * the plane. For `OffsetFromCamera`, this is the camera-space offset transformed into model space. It is ignored for
   * `FrameOrigin`.
   */
  void setPositioningMethod(
    const intersection::PositioningMethod& method,
    const std::optional<glm::vec3>& positionOrCameraOffset = std::nullopt);

  const intersection::PositioningMethod& positioningMethod() const;

  /**
   * @brief Select how the slice plane normal is derived.
   *
   * @param method Alignment mode.
   * @param worldNormal Optional normal used only for `UserDefined`. Nonzero normals are normalized before storage.
   */
  void setAlignmentMethod(
    const intersection::AlignmentMethod& method,
    const std::optional<glm::vec3>& worldNormal = std::nullopt);

  const intersection::AlignmentMethod& alignmentMethod() const;

  /**
   * @brief Compute the current box-plane intersection and active model-space plane equation.
   *
   * @param model_T_camera Transform from camera space into model space.
   * @param model_T_frame Transform from the selected frame space into model space.
   * @param modelBoxCorners Eight model-space corners of the box being intersected.
   * @return Intersection vertices when the plane cuts the box, plus the model-space plane equation.
   */
  std::pair<std::optional<intersection::IntersectionVertices>, glm::vec4> computePlaneIntersections(
    const glm::mat4& model_O_camera,
    const glm::mat4& model_O_frame,
    const std::array<glm::vec3, 8>& modelBoxCorners);

private:
  void updatePlaneEquation(const glm::mat4& model_T_camera, const glm::mat4& model_T_frame);

  intersection::PositioningMethod m_positioningMethod;
  intersection::AlignmentMethod m_alignmentMethod;

  glm::vec3 m_cameraSliceOffset;
  glm::vec3 m_userSlicePosition;
  glm::vec3 m_userSliceNormal;

  glm::vec4 m_modelPlaneEquation;
};
