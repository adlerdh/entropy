#pragma once

#include <glm/vec3.hpp>

#include <array>
#include <vector>

class Camera;

namespace camera3d
{

/**
 * @brief Line segment where one face of a 3D camera frustum intersects a 2D slice plane.
 */
struct FrustumSliceSegment
{
  enum class Face
  {
    Near,
    Far,
    Left,
    Right,
    Top,
    Bottom
  };

  Face face = Face::Near;
  glm::vec3 a{0.0f};
  glm::vec3 b{0.0f};
};

/**
 * @brief World-space geometry used to draw a 3D camera eye/frustum in a 2D view.
 */
struct FrustumSliceOverlay
{
  glm::vec3 eye{0.0f};
  std::vector<FrustumSliceSegment> segments;
};

/**
 * @brief Intersect a 3D camera frustum with a world-space plane.
 * @param camera 3D camera whose near/far frustum should be visualized.
 * @param planePoint Point on the 2D view plane in world coordinates.
 * @param planeNormal Normal of the 2D view plane in world coordinates.
 * @return Eye position plus the frustum-face line segments lying in the plane.
 */
FrustumSliceOverlay
frustumSliceOverlay(const Camera& camera, const glm::vec3& planePoint, const glm::vec3& planeNormal);

} // namespace camera3d
