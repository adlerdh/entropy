#ifndef SLICE_INTERSECTOR_H
#define SLICE_INTERSECTOR_H

#include "rendering/utility/math/SliceIntersectorTypes.h"

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <optional>
#include <utility>

/**
 * @brief Intersects a box (with vertices defined in local Modeling coordinate space)
 * against a plane.
 */
class SliceIntersector
{
public:
  static constexpr int s_numIntersections = 6;
  static constexpr int s_numVertices = 7;

  explicit SliceIntersector();

  SliceIntersector(const SliceIntersector&) = default;
  SliceIntersector(SliceIntersector&&) = default;

  SliceIntersector& operator=(const SliceIntersector&) = default;
  SliceIntersector& operator=(SliceIntersector&&) = default;

  ~SliceIntersector() = default;

  void setPositioningMethod(
    const intersection::PositioningMethod& method, const std::optional<glm::vec3>& p = std::nullopt
  );

  const intersection::PositioningMethod& positioningMethod() const;

  void setAlignmentMethod(
    const intersection::AlignmentMethod& method,
    const std::optional<glm::vec3>& worldNormal = std::nullopt
  );

  const intersection::AlignmentMethod& alignmentMethod() const;

  /// Compute and return the intersection vertices (if they exist) and the plane equation
  std::pair<std::optional<intersection::IntersectionVertices>, glm::vec4> computePlaneIntersections(
    const glm::mat4& model_T_camera,
    const glm::mat4& model_T_frame,
    const std::array<glm::vec3, 8>& modelBoxCorners
  );

private:
  void updatePlaneEquation(const glm::mat4& model_O_camera, const glm::mat4& model_O_frame);

  intersection::PositioningMethod m_positioningMethod;
  intersection::AlignmentMethod m_alignmentMethod;

  glm::vec3 m_cameraSliceOffset;
  glm::vec3 m_userSlicePosition;
  glm::vec3 m_userSliceNormal;

  glm::vec4 m_modelPlaneEquation;
};

#endif // SLICE_INTERSECTOR_H
