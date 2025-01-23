#ifndef MATH_UTILITY_H
#define MATH_UTILITY_H

#include "common/AABB.h"
#include "common/Types.h"

#include <glm/glm.hpp>
#include <glm/vector_relational.hpp>

#include <glm/gtc/epsilon.hpp>
#include <glm/gtc/matrix_access.hpp>
#include <glm/gtc/matrix_transform.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/component_wise.hpp>
#include <glm/gtx/string_cast.hpp>

#include <spdlog/fmt/ostr.h>
#include <spdlog/spdlog.h>

#include <boost/range/any_range.hpp>

#include <array>
#include <cmath>
#include <limits>
#include <numeric>
#include <optional>
#include <tuple>
#include <unordered_map>
#include <vector>

#undef min
#undef max

class Viewport;

namespace camera
{
class Camera;
}

/// @note These math functions mostly come from HistoloZee.
namespace math
{

template<typename T>
using gvec3 = glm::vec<3, T, glm::highp>;

template<typename T>
using gvec4 = glm::vec<4, T, glm::highp>;

template<typename T>
using gmat3 = glm::mat<3, 3, T, glm::highp>;

template<typename T>
using gmat4 = glm::mat<4, 4, T, glm::highp>;

template<class U>
using range = boost::any_range<U, boost::forward_traversal_tag, U&, std::ptrdiff_t>;

/**
 * @brief Build an orthonormal basis from a single vector. Use this to create a camera basis with a
 * lookat direction without any priority axes
 *
 * @see "Building an Orthonormal Basis, Revisited" by Tom Duff, James Burgess, Per Christensen,
 * Christophe Hery, Andrew Kensler, Max Liani, and Ryusuke Villemin.
 * Journal of Computer Graphics Techniques Vol. 6, No. 1, 2017
 */
std::pair<glm::vec3, glm::vec3> buildOrthonormalBasis_branchless(const glm::vec3& n);
std::pair<glm::vec3, glm::vec3> buildOrthonormalBasis(const glm::vec3& n);

glm::vec3 convertVecToRGB(const glm::vec3& v);

glm::u8vec3 convertVecToRGB_uint8(const glm::vec3& v);

std::vector<uint32_t> sortCounterclockwise(const std::vector<glm::vec2>& points);

std::vector<glm::vec2> project3dPointsToPlane(const std::vector<glm::vec3>& A);

glm::vec3 projectPointToPlane(const glm::vec3& point, const glm::vec4& planeEquation);

/**
 * @brief Project a 3D point into a plane and return the point's local 2D plane coordinates
 * @param[in] point Point in 3D
 * @param[in] planeEquation Plane equation coefficients (A, B, C, D) where Ax + By + Cz + D = 0
 * @param[in] planeOrigin Plane origin in 3D
 * @param[in] planeAxes Plane axes in 3D that define the local plane coordinate system
 * @return The point expressed in local 2D plane coordinates after being projected into the plane
 */
glm::vec2 projectPointToPlaneLocal2dCoords(
  const glm::vec3& point,
  const glm::vec4& planeEquation,
  const glm::vec3& planeOrigin,
  const std::pair<glm::vec3, glm::vec3>& planeAxes
);

/**
 * @brief Add offsets to vertex positions of an object (defined in its own Model space)
 * in order to account for its layering. This function is used when rendering "flat"
 * objects in 2D views.
 *
 * @param[in] camera Camera of the view in which th object is rendered
 * @param[in] model_T_world Transformation from World to Model space
 * @param[in] layer Layer of the model
 * @param[in,out] modelPositions Model-space vertex positions that are modified
 */
void applyLayeringOffsetsToModelPositions(
  const camera::Camera& camera,
  const glm::mat4& model_T_world,
  uint32_t layer,
  std::vector<glm::vec3>& modelPositions
);

template<typename T>
gmat4<T> fromToRotation(const gvec3<T>& fromVec, const gvec3<T>& toVec)
{
  gmat4<T> R(T(1.0));

  gvec3<T> v = glm::cross(fromVec, toVec);
  T e = glm::dot(fromVec, toVec);
  T f = (e < T(0.0)) ? -e : e;

  if (f > T(1.0) - glm::epsilon<T>())
  {
    /// "from" and "to"-vector almost parallel

    /// vector most nearly orthogonal to "from"
    gvec3<T> x = glm::abs(fromVec);

    if (x[0] < x[1])
    {
      if (x[0] < x[2])
      {
        x[0] = T(1);
        x[1] = x[2] = T(0);
      }
      else
      {
        x[2] = T(1);
        x[0] = x[1] = T(0);
      }
    }
    else
    {
      if (x[1] < x[2])
      {
        x[1] = T(1);
        x[0] = x[2] = T(0);
      }
      else
      {
        x[2] = T(1);
        x[0] = x[1] = T(0);
      }
    }

    gvec3<T> u = x - fromVec;
    gvec3<T> v_temp = x - toVec;

    T c1 = T(2.0) / glm::dot(u, u);
    T c2 = T(2.0) / glm::dot(v_temp, v_temp);
    T c3 = c1 * c2 * glm::dot(u, v_temp);

    for (int row = 0; row < 3; ++row)
    {
      for (int col = 0; col < 3; ++col)
      {
        R[col][row] = -c1 * u[row] * u[col] - c2 * v_temp[row] * v_temp[col]
                      + c3 * v_temp[row] * u[col];
      }

      R[row][row] += T(1);
    }
  }
  else
  {
    /// the most common case, unless "from" == "to", or "from" == -"to"

    T h = T(1.0) / (T(1.0) + e);

    T hvx = h * v[0];
    T hvz = h * v[2];
    T hvxy = hvx * v[1];
    T hvxz = hvx * v[2];
    T hvyz = hvz * v[1];

    R[0][0] = e + hvx * v[0];
    R[1][0] = hvxy - v[2];
    R[2][0] = hvxz + v[1];

    R[0][1] = hvxy + v[2];
    R[1][1] = e + h * v[1] * v[1];
    R[2][1] = hvyz - v[0];

    R[0][2] = hvxz - v[1];
    R[1][2] = hvyz + v[0];
    R[2][2] = e + hvz * v[2];
  }

  return R;
}

template<typename T>
int sgn(const T& val)
{
  return (T(0) < val) - (val < T(0));
}

template<typename T>
bool areMatricesEqual(const gmat3<T>& A, const gmat3<T>& B)
{
  static constexpr float EPS = glm::epsilon<float>();

  if (glm::any(glm::epsilonNotEqual(A[0], B[0], EPS)) || glm::any(glm::epsilonNotEqual(A[1], B[1], EPS)) || glm::any(glm::epsilonNotEqual(A[2], B[2], EPS)))
  {
    return false;
  }

  return true;
}

template<typename T>
bool areMatricesEqual(const gmat4<T>& A, const gmat4<T>& B, float epsilon = glm::epsilon<float>())
{
  if (glm::any(glm::epsilonNotEqual(A[0], B[0], epsilon)) || glm::any(glm::epsilonNotEqual(A[1], B[1], epsilon)) || glm::any(glm::epsilonNotEqual(A[2], B[2], epsilon)) || glm::any(glm::epsilonNotEqual(A[3], B[3], epsilon)))
  {
    return false;
  }

  return true;
}

/**
 * @brief Create plane (A, B, C, D) in form Ax + By + Cz + D = 0
 * with given normal vector (A, B, C) and passing through a given point.
 */
template<typename T>
gvec4<T> makePlane(const gvec3<T>& normal, const gvec3<T>& point)
{
  return gvec4<T>{normal, -glm::dot(normal, point)};
}

/**
 * @brief Compute the axis-aligned bounding box of a range of points.
 */
template<typename T>
AABB<T> computeAABBox(range<gvec3<T> > points)
{
  AABB<T> minMaxCorners = std::make_pair(
    gvec3<T>(std::numeric_limits<T>::max()), gvec3<T>(std::numeric_limits<T>::lowest())
  );

  for (const auto& point : points)
  {
    minMaxCorners.first = glm::min(minMaxCorners.first, point);
    minMaxCorners.second = glm::max(minMaxCorners.second, point);
  }

  return minMaxCorners;
}

/**
 * @brief Compute the eight corners of an axis-aligned bounding box.
 */
template<typename T>
std::array<gvec3<T>, 8> makeAABBoxCorners(const AABB<T>& box)
{
  const gvec3<T> diag = box.second - box.first;

  return {
    box.first,
    box.first + gvec3<T>{diag.x, 0, 0},
    box.first + gvec3<T>{0, diag.y, 0},
    box.first + gvec3<T>{0, 0, diag.z},
    box.first + gvec3<T>{diag.x, diag.y, 0},
    box.first + gvec3<T>{diag.x, 0, diag.z},
    box.first + diag,
    box.second
  };
}

template<typename T>
gvec3<T> computeAABBoxCenter(const AABB<T>& box)
{
  return static_cast<T>(0.5) * (box.first + box.second);
}

template<typename T>
gvec3<T> computeAABBoxSize(const AABB<T>& box)
{
  return glm::abs(box.second - box.first);
}

template<typename T>
bool isInside(const AABB<T>& box, const gvec3<T>& point)
{
  return (
    glm::all(glm::lessThanEqual(box.first, point)) && glm::all(glm::lessThanEqual(point, box.second))
  );
}

/**
 * @brief Compute the axis-aligned bounding box (AABB) that bounds
 * two other AABBs.
 */
template<typename T>
AABB<T> computeBoundingAABBox(const AABB<T> box1, const AABB<T> box2)
{
  return {glm::min(box1.first, box2.first), glm::max(box1.second, box2.second)};
}

template<typename T>
bool testAABBoxPlaneIntersection(
  const gvec3<T>& boxCenter, const gvec3<T>& boxMaxCorner, const gvec4<T>& plane
)
{
  const gvec3<T> extent = boxMaxCorner - boxCenter;

  const T radius = glm::dot(extent, glm::abs(gvec3<T>{plane}));

  // Distance of AABB center from plane
  const T dist = glm::dot(plane, gvec4<T>{boxCenter, 1});

  // Intersection occurs when distance is in [-radius, +radius]
  return (std::abs(dist) <= radius);
}

// Return true if intersection, false if not
template<typename T>
bool computeSortedAABBoxCorners(
  const std::array<gvec3<T>, 8>& corners,
  const gvec4<T>& plane,
  std::array<gvec3<T>, 8>& sortedCorners
)
{
  static const std::unordered_map<int, int> sk_nearFarCornerMap
    = {{0, 7}, {1, 6}, {2, 5}, {3, 4}, {4, 3}, {5, 2}, {6, 1}, {7, 0}};

  T minDistance = std::numeric_limits<T>::max();
  T maxDistance = std::numeric_limits<T>::lowest();

  int nearCornerIndex = 0;

  for (int i = 0; i < 8; ++i)
  {
    T distance = glm::dot(gvec4<T>{corners[i], 1}, plane);

    if (distance < minDistance)
    {
      minDistance = distance;
      nearCornerIndex = i;
    }

    if (distance > maxDistance)
    {
      maxDistance = distance;
    }
  }

  if (sgn(minDistance) == sgn(maxDistance))
  {
    //        std::cerr << "There is no intersection!" << std::endl;
    return false;
  }

  const int farthestCornerIndex = sk_nearFarCornerMap.at(nearCornerIndex);

  const gvec3<T> closestCorner = corners[nearCornerIndex];
  const gvec3<T> farthestCorner = corners[farthestCornerIndex];
  const gvec3<T> cornerDelta = farthestCorner - closestCorner;

  /// @see AABB corners sorted according to the paper
  /// Rezk Salama & Kolb, "A Vertex Program for Efficient Box-Plane Intersection", VMV 2005.
  sortedCorners[0] = closestCorner;
  sortedCorners[1] = closestCorner + gvec3<T>{cornerDelta.x, 0, 0};
  sortedCorners[2] = closestCorner + gvec3<T>{0, cornerDelta.y, 0};
  sortedCorners[3] = closestCorner + gvec3<T>{0, 0, cornerDelta.z};
  sortedCorners[4] = sortedCorners[1] + gvec3<T>{0, 0, cornerDelta.z};
  sortedCorners[5] = sortedCorners[2] + gvec3<T>{cornerDelta.x, 0, 0};
  sortedCorners[6] = sortedCorners[3] + gvec3<T>{0, cornerDelta.y, 0};
  sortedCorners[7] = farthestCorner;

  return true;
}

template<typename T>
bool lineSegmentPlaneIntersection(
  const gvec3<T>& lineStartPoint,
  const gvec3<T>& lineEndPoint,
  const gvec4<T>& plane,
  T& intersectionDistance
)
{
  static const T sk_eps = glm::epsilon<T>();

  const T denom = glm::dot(plane, gvec4<T>{lineEndPoint - lineStartPoint, 0});

  if (std::abs(denom) > sk_eps)
  {
    intersectionDistance = -glm::dot(plane, gvec4<T>{lineStartPoint, 1}) / denom;

    if (T(0) <= intersectionDistance && intersectionDistance <= T(1))
    {
      return true;
    }
  }

  return false;
}

template<typename T>
bool vectorPlaneIntersection(
  const gvec3<T>& lineStartPoint,
  const gvec3<T>& lineDirection,
  const gvec4<T>& plane,
  T& intersectionDistance
)
{
  static const T sk_eps = glm::epsilon<T>();

  const T denom = glm::dot(plane, gvec4<T>{lineDirection, 0});

  if (std::abs(denom) > sk_eps)
  {
    intersectionDistance = -glm::dot(plane, gvec4<T>{lineStartPoint, 1}) / denom;
    return true;
  }

  return false;
}

// Last intersection is average
template<typename T>
std::optional<std::array<gvec3<T>, 7> > computeSliceIntersections(
  const std::array<gvec3<T>, 8>& sortedCorners, const gvec4<T>& plane
)
{
  std::array<gvec3<T>, 7> intersections;

  // Average of the intersection points
  gvec3<T> intersectionAverage(0, 0, 0);

  int count = 0;

  T t = 0.0;

  if (lineSegmentPlaneIntersection(sortedCorners[0], sortedCorners[1], plane, t))
  {
    intersections[0] = sortedCorners[0] + t * (sortedCorners[1] - sortedCorners[0]);
  }
  else if (lineSegmentPlaneIntersection(sortedCorners[1], sortedCorners[4], plane, t))
  {
    intersections[0] = sortedCorners[1] + t * (sortedCorners[4] - sortedCorners[1]);
  }
  else if (lineSegmentPlaneIntersection(sortedCorners[4], sortedCorners[7], plane, t))
  {
    intersections[0] = sortedCorners[4] + t * (sortedCorners[7] - sortedCorners[4]);
  }
  else
  {
    return std::nullopt;
  }

  if (lineSegmentPlaneIntersection(sortedCorners[0], sortedCorners[2], plane, t))
  {
    intersections[2] = sortedCorners[0] + t * (sortedCorners[2] - sortedCorners[0]);
  }
  else if (lineSegmentPlaneIntersection(sortedCorners[2], sortedCorners[5], plane, t))
  {
    intersections[2] = sortedCorners[2] + t * (sortedCorners[5] - sortedCorners[2]);
  }
  else if (lineSegmentPlaneIntersection(sortedCorners[5], sortedCorners[7], plane, t))
  {
    intersections[2] = sortedCorners[5] + t * (sortedCorners[7] - sortedCorners[5]);
  }
  else
  {
    return std::nullopt;
  }

  if (lineSegmentPlaneIntersection(sortedCorners[0], sortedCorners[3], plane, t))
  {
    intersections[4] = sortedCorners[0] + t * (sortedCorners[3] - sortedCorners[0]);
  }
  else if (lineSegmentPlaneIntersection(sortedCorners[3], sortedCorners[6], plane, t))
  {
    intersections[4] = sortedCorners[3] + t * (sortedCorners[6] - sortedCorners[3]);
  }
  else if (lineSegmentPlaneIntersection(sortedCorners[6], sortedCorners[7], plane, t))
  {
    intersections[4] = sortedCorners[6] + t * (sortedCorners[7] - sortedCorners[6]);
  }
  else
  {
    return std::nullopt;
  }

  intersectionAverage += intersections[0];
  intersectionAverage += intersections[2];
  intersectionAverage += intersections[4];

  count = 3;

  /// @internal As in Rezk Salama & Kolb, duplicate the intersections
  /// to ensure a total count of 6
  if (lineSegmentPlaneIntersection(sortedCorners[1], sortedCorners[5], plane, t))
  {
    intersections[1] = sortedCorners[1] + t * (sortedCorners[5] - sortedCorners[1]);
    intersectionAverage += intersections[1];
    ++count;
  }
  else
  {
    intersections[1] = intersections[0];
  }

  if (lineSegmentPlaneIntersection(sortedCorners[2], sortedCorners[6], plane, t))
  {
    intersections[3] = sortedCorners[2] + t * (sortedCorners[6] - sortedCorners[2]);
    intersectionAverage += intersections[3];
    ++count;
  }
  else
  {
    intersections[3] = intersections[2];
  }

  if (lineSegmentPlaneIntersection(sortedCorners[3], sortedCorners[4], plane, t))
  {
    intersections[5] = sortedCorners[3] + t * (sortedCorners[4] - sortedCorners[3]);
    intersectionAverage += intersections[5];
    ++count;
  }
  else
  {
    intersections[5] = intersections[4];
  }

  intersectionAverage = intersectionAverage / static_cast<T>(count);
  intersections[6] = intersectionAverage;

  return intersections;
}

template<typename T>
std::optional<std::array<gvec3<T>, 7> > computeAABBoxPlaneIntersections(
  const std::array<gvec3<T>, 8>& boxCorners, const gvec4<T>& plane
)
{
  gvec3<T> boxCenter(0, 0, 0);
  gvec3<T> boxMaxCorner(std::numeric_limits<T>::lowest());

  for (const auto& corner : boxCorners)
  {
    boxCenter += corner;
    boxMaxCorner = glm::max(boxMaxCorner, corner);
  }

  boxCenter /= static_cast<T>(8.0);

  if (!testAABBoxPlaneIntersection(boxCenter, boxMaxCorner, plane))
  {
    return std::nullopt;
  }

  std::array<gvec3<T>, 8> sortedCorners;

  if (!computeSortedAABBoxCorners(boxCorners, plane, sortedCorners))
  {
    return std::nullopt;
  }

  return computeSliceIntersections(sortedCorners, plane);
}

template<size_t N>
std::array<float, N> computeLayerBlendWeights(const std::array<float, N>& layerOpacities)
{
  std::array<float, N> weights = layerOpacities;

  for (size_t i = 0; i < N; ++i)
  {
    for (size_t j = i + 1; j < N; ++j)
    {
      weights[i] *= (1.0f - layerOpacities[j]);
    }
  }

  return weights;
}

template<size_t N>
float computeOverallOpacity(const std::array<float, N>& layerOpacities)
{
  const std::array<float, N> weights = computeLayerBlendWeights(layerOpacities);

  return std::accumulate(std::begin(weights), std::end(weights), 0.0f);
}

template<typename T>
std::optional<gvec3<T> > intersectRayWithAABBox(
  const gvec3<T>& rayOrig, const gvec3<T>& rayDir, const gvec3<T>& boxMin, const gvec3<T>& boxMax
)
{
  const gvec3<T> tmin = (boxMin - rayOrig) / rayDir;
  const gvec3<T> tmax = (boxMax - rayOrig) / rayDir;

  const float minmax = glm::compMin(glm::min(tmin, tmax));
  const float maxmin = glm::compMax(glm::max(tmin, tmax));

  if (minmax >= maxmin)
  {
    return rayOrig + maxmin * rayDir;
  }

  return std::nullopt;
}

/**
 * @brief Signed distance from 3D point to plane
 * @param point 3D point
 * @param plane 3D plane expressed as (a, b, c, d), where ax + by + cz + d = 0
 * @return Positive distance if point is on same side of plane as normal vector;
 * negative if on the other side.
 */
template<typename T>
T signedDistancePointToPlane(const gvec3<T>& point, const gvec4<T>& plane)
{
  const gvec4<T> p{point, 1.0};
  return glm::dot(plane, p);
}

/**
 * @brief For a given axis-aligned bounding box and a plane, compute the
 * corner of the box farthest from the plane on its negative side
 * (call this the "near" corner) and the corner of the box farthest from the
 * plane on its positive side (call this the "far" corner)
 *
 * @param boxCorners Array of the eight AABB corners
 * @param plane 3D plane expressed as (a, b, c, d), where ax + by + cz + d = 0
 *
 * @return The pair of nearest and farther corners of the AABB with respect to
 * the given plane
 */
template<typename T>
std::tuple<gvec3<T>, T, gvec3<T>, T> computeNearAndFarAABBoxCorners(
  const std::array<gvec3<T>, 8>& boxCorners, const gvec4<T>& plane
)
{
  T nearCornerDistance = std::numeric_limits<T>::max();
  T farCornerDistance = std::numeric_limits<T>::lowest();

  gvec3<T> nearCorner = boxCorners[0];
  gvec3<T> farCorner = boxCorners[1];

  for (const auto& corner : boxCorners)
  {
    const T dist = signedDistancePointToPlane(corner, plane);

    if (dist < nearCornerDistance)
    {
      nearCornerDistance = dist;
      nearCorner = corner;
    }

    if (dist > farCornerDistance)
    {
      farCornerDistance = dist;
      farCorner = corner;
    }
  }

  return {nearCorner, nearCornerDistance, farCorner, farCornerDistance};
}

/**
 * @brief Compute the left, posterior, and superior directions of the subject in Camera space.
 *
 * @param[in] camera_T_world_rotation Rotation matrix from World to Camera space
 * @param[in] world_T_subject_rotation Rotation matrix from Subject to World space
 *
 * @return Left, posterior, and superior directions of the Subject in Camera space
 */
glm::mat3 computeSubjectAxesInCamera(
  const glm::mat3& camera_T_world_rotation, const glm::mat3& world_T_subject_rotation
);

/**
 * @brief Compute the equation of the view plane in Subject space
 * @param subject_T_world
 * @param worldPlaneNormal
 * @param worldPlanePoint
 * @return
 */
std::pair<glm::vec4, glm::vec3> computeSubjectPlaneEquation(
  const glm::mat4 subject_T_world,
  const glm::vec3& worldPlaneNormal,
  const glm::vec3& worldPlanePoint
);

std::array<AnatomicalLabelPosInfo, 2> computeAnatomicalLabelsForView(
  const glm::mat4& camera_T_world, const glm::mat4& world_T_subject
);

std::array<AnatomicalLabelPosInfo, 2> computeAnatomicalLabelPosInfo(
  const FrameBounds& miewportViewBounds,
  const Viewport& windowVP,
  const camera::Camera& camera,
  const glm::mat4& world_T_subject,
  const glm::mat4& windowClip_T_viewClip,
  const glm::vec3& worldCrosshairsPos
);

} // namespace math

#endif // MATH_UTILITY_H
