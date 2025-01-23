#ifndef SLICE_INTERSECTOR_TYPES_H
#define SLICE_INTERSECTOR_TYPES_H

#include <glm/fwd.hpp>

#include <array>

namespace intersection
{

/**
 * @brief Describes the method used for positioning slices
 */
enum class PositioningMethod
{
  OffsetFromCamera,
  FrameOrigin,
  UserDefined
};

/**
 * @brief Describes the method used for aligning slices
 */
enum class AlignmentMethod
{
  CameraZ,
  FrameX,
  FrameY,
  FrameZ,
  UserDefined
};

// There are up to six intersection points between a 3D plane and a 3D AABB.
// We store the intersection polygon in vertex buffer using seven vertices:
// Six are the intersection vertices themselves (included repeated ones);
// plus one more hub vertex at the centroid of the intersection points.

using IntersectionVertices = std::array<glm::vec3, 7>;
using IntersectionVerticesVec4 = std::array<glm::vec4, 7>;

} // namespace intersection

#endif // SLICE_INTERSECTOR_TYPES_H
