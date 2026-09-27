#pragma once

#include "common/AABB.h"
#include "common/MathFuncs.h"

#include <glm/common.hpp>
#include <glm/geometric.hpp>
#include <glm/gtc/epsilon.hpp>
#include <glm/mat3x3.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

#include <array>
#include <cmath>
#include <limits>
#include <optional>
#include <ranges>
#include <unordered_map>

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

/**
 * @brief Return the sign of a scalar value.
 * @return -1 when `val` is negative, 1 when `val` is positive, and 0 when `val` is equal to zero.
 */
template<typename T>
int sgn(const T& val)
{
  return (T(0) < val) - (val < T(0));
}

/**
 * @brief Create a plane `(A, B, C, D)` from a normal and point on the plane.
 *
 * The resulting plane equation is `Ax + By + Cz + D = 0`, where `(A, B, C)` is copied from `normal`
 * and `D` is chosen so that `point` lies on the plane.
 *
 * @pre `normal` should be non-zero. The function does not normalize it.
 */
template<typename T>
gvec4<T> makePlane(const gvec3<T>& normal, const gvec3<T>& point)
{
  return gvec4<T>{normal, -glm::dot(normal, point)};
}

/**
 * @brief Compute the axis-aligned bounding box of a range of 3D points.
 *
 * @param points Forward range of values comparable with `glm::min`/`glm::max`.
 * @return A pair of minimum and maximum corners.
 *
 * Empty ranges return sentinel numeric limits: max values for the minimum corner and lowest values
 * for the maximum corner. Callers that require a valid physical box should reject empty inputs
 * before calling.
 */
template<typename T, std::ranges::forward_range PointRange>
AABB<T> computeAABBox(const PointRange& points)
{
  AABB<T> minMaxCorners =
    std::make_pair(gvec3<T>(std::numeric_limits<T>::max()), gvec3<T>(std::numeric_limits<T>::lowest()));

  for (const auto& point : points) {
    minMaxCorners.first = glm::min(minMaxCorners.first, point);
    minMaxCorners.second = glm::max(minMaxCorners.second, point);
  }

  return minMaxCorners;
}

/**
 * @brief Compute the eight corners of an axis-aligned bounding box.
 *
 * The returned order is stable but is not a winding order. Use `computeSortedAABBoxCorners()` when
 * a plane-relative corner ordering is required for slice intersection.
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
    box.second};
}

/**
 * @brief Compute the center point of an axis-aligned bounding box.
 */
template<typename T>
gvec3<T> computeAABBoxCenter(const AABB<T>& box)
{
  return static_cast<T>(0.5) * (box.first + box.second);
}

/**
 * @brief Compute the absolute size vector of an axis-aligned bounding box.
 */
template<typename T>
gvec3<T> computeAABBoxSize(const AABB<T>& box)
{
  return glm::abs(box.second - box.first);
}

/**
 * @brief Test whether a point is inside an axis-aligned bounding box.
 * @return true when `point` is inside or on the boundary of `box`.
 */
template<typename T>
bool isInside(const AABB<T>& box, const gvec3<T>& point)
{
  return (glm::all(glm::lessThanEqual(box.first, point)) && glm::all(glm::lessThanEqual(point, box.second)));
}

/**
 * @brief Compute the smallest axis-aligned bounding box that contains two AABBs.
 */
template<typename T>
AABB<T> computeBoundingAABBox(const AABB<T> box1, const AABB<T> box2)
{
  return {glm::min(box1.first, box2.first), glm::max(box1.second, box2.second)};
}

/**
 * @brief Sort AABB corners according to near/far distance from a plane.
 *
 * This produces the plane-relative corner order expected by `computeSliceIntersections()`, following
 * the box-plane intersection strategy from Salama and Kolb.
 *
 * @param corners The eight AABB corners from `makeAABBoxCorners()`.
 * @param plane Plane equation `(A, B, C, D)` where `Ax + By + Cz + D = 0`.
 * @param[out] sortedCorners Plane-relative corner order used by the slice-intersection algorithm.
 * @return true when the plane crosses or touches the AABB; false when all corners are strictly on
 * one side of the plane.
 */
template<typename T>
bool computeSortedAABBoxCorners(
  const std::array<gvec3<T>, 8>& corners,
  const gvec4<T>& plane,
  std::array<gvec3<T>, 8>& sortedCorners)
{
  static const std::unordered_map<int, int> sk_nearFarCornerMap =
    {{0, 7}, {1, 6}, {2, 5}, {3, 4}, {4, 3}, {5, 2}, {6, 1}, {7, 0}};

  T minDistance = std::numeric_limits<T>::max();
  T maxDistance = std::numeric_limits<T>::lowest();

  int nearCornerIndex = 0;

  for (int i = 0; i < 8; ++i) {
    T distance = glm::dot(gvec4<T>{corners[i], 1}, plane);

    if (distance < minDistance) {
      minDistance = distance;
      nearCornerIndex = i;
    }

    if (distance > maxDistance) {
      maxDistance = distance;
    }
  }

  if (sgn(minDistance) == sgn(maxDistance)) {
    return false;
  }

  const int farthestCornerIndex = sk_nearFarCornerMap.at(nearCornerIndex);

  const gvec3<T> closestCorner = corners[nearCornerIndex];
  const gvec3<T> farthestCorner = corners[farthestCornerIndex];
  const gvec3<T> cornerDelta = farthestCorner - closestCorner;

  // Corner order from Rezk Salama and Kolb, "A Vertex Program for Efficient Box-Plane Intersection", VMV 2005.
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

/**
 * @brief Intersect a finite line segment with a plane.
 *
 * @param lineStartPoint Segment start point.
 * @param lineEndPoint Segment end point.
 * @param plane Plane equation `(A, B, C, D)` where `Ax + By + Cz + D = 0`.
 * @param[out] intersectionDistance Parametric distance from start to end in `[0, 1]`.
 * @return true when the segment intersects the plane at a single point. Parallel or coplanar
 * segments and intersections outside the segment return false.
 */
template<typename T>
bool lineSegmentPlaneIntersection(
  const gvec3<T>& lineStartPoint,
  const gvec3<T>& lineEndPoint,
  const gvec4<T>& plane,
  T& intersectionDistance)
{
  static const T sk_eps = glm::epsilon<T>();

  const T denom = glm::dot(plane, gvec4<T>{lineEndPoint - lineStartPoint, 0});

  if (std::abs(denom) > sk_eps) {
    intersectionDistance = -glm::dot(plane, gvec4<T>{lineStartPoint, 1}) / denom;

    if (T(0) <= intersectionDistance && intersectionDistance <= T(1)) {
      return true;
    }
  }

  return false;
}

/**
 * @brief Intersect an infinite directed line with a plane.
 *
 * @param lineStartPoint Start point of the directed line.
 * @param lineDirection Direction vector of the line.
 * @param plane Plane equation `(A, B, C, D)` where `Ax + By + Cz + D = 0`.
 * @param[out] intersectionDistance Signed parametric distance along `lineDirection`.
 * @return true when the direction is not parallel to the plane. Parallel or coplanar lines return
 * false.
 */
template<typename T>
bool vectorPlaneIntersection(
  const gvec3<T>& lineStartPoint,
  const gvec3<T>& lineDirection,
  const gvec4<T>& plane,
  T& intersectionDistance)
{
  static const T sk_eps = glm::epsilon<T>();

  const T denom = glm::dot(plane, gvec4<T>{lineDirection, 0});

  if (std::abs(denom) > sk_eps) {
    intersectionDistance = -glm::dot(plane, gvec4<T>{lineStartPoint, 1}) / denom;
    return true;
  }

  return false;
}

/**
 * @brief Compute a seven-vertex polygon for a plane crossing sorted AABB corners.
 *
 * The first six vertices describe the intersection polygon, with duplicates inserted when fewer
 * unique vertices are present. The seventh vertex is the centroid of the unique intersections. This
 * fixed-size representation matches the GPU upload shape used by the slice renderer.
 *
 * @param sortedCorners Plane-relative AABB corners from `computeSortedAABBoxCorners()`.
 * @param plane Plane equation `(A, B, C, D)` where `Ax + By + Cz + D = 0`.
 * @return The seven-vertex intersection representation, or `std::nullopt` when the expected
 * intersection edges cannot be found.
 */
template<typename T>
std::optional<std::array<gvec3<T>, 7> > computeSliceIntersections(
  const std::array<gvec3<T>, 8>& sortedCorners,
  const gvec4<T>& plane)
{
  std::array<gvec3<T>, 7> intersections;
  gvec3<T> intersectionAverage(0, 0, 0);

  int count = 0;
  T t = 0.0;

  if (lineSegmentPlaneIntersection(sortedCorners[0], sortedCorners[1], plane, t)) {
    intersections[0] = sortedCorners[0] + t * (sortedCorners[1] - sortedCorners[0]);
  }
  else if (lineSegmentPlaneIntersection(sortedCorners[1], sortedCorners[4], plane, t)) {
    intersections[0] = sortedCorners[1] + t * (sortedCorners[4] - sortedCorners[1]);
  }
  else if (lineSegmentPlaneIntersection(sortedCorners[4], sortedCorners[7], plane, t)) {
    intersections[0] = sortedCorners[4] + t * (sortedCorners[7] - sortedCorners[4]);
  }
  else {
    return std::nullopt;
  }

  if (lineSegmentPlaneIntersection(sortedCorners[0], sortedCorners[2], plane, t)) {
    intersections[2] = sortedCorners[0] + t * (sortedCorners[2] - sortedCorners[0]);
  }
  else if (lineSegmentPlaneIntersection(sortedCorners[2], sortedCorners[5], plane, t)) {
    intersections[2] = sortedCorners[2] + t * (sortedCorners[5] - sortedCorners[2]);
  }
  else if (lineSegmentPlaneIntersection(sortedCorners[5], sortedCorners[7], plane, t)) {
    intersections[2] = sortedCorners[5] + t * (sortedCorners[7] - sortedCorners[5]);
  }
  else {
    return std::nullopt;
  }

  if (lineSegmentPlaneIntersection(sortedCorners[0], sortedCorners[3], plane, t)) {
    intersections[4] = sortedCorners[0] + t * (sortedCorners[3] - sortedCorners[0]);
  }
  else if (lineSegmentPlaneIntersection(sortedCorners[3], sortedCorners[6], plane, t)) {
    intersections[4] = sortedCorners[3] + t * (sortedCorners[6] - sortedCorners[3]);
  }
  else if (lineSegmentPlaneIntersection(sortedCorners[6], sortedCorners[7], plane, t)) {
    intersections[4] = sortedCorners[6] + t * (sortedCorners[7] - sortedCorners[6]);
  }
  else {
    return std::nullopt;
  }

  intersectionAverage += intersections[0];
  intersectionAverage += intersections[2];
  intersectionAverage += intersections[4];
  count = 3;

  if (lineSegmentPlaneIntersection(sortedCorners[1], sortedCorners[5], plane, t)) {
    intersections[1] = sortedCorners[1] + t * (sortedCorners[5] - sortedCorners[1]);
    intersectionAverage += intersections[1];
    ++count;
  }
  else {
    intersections[1] = intersections[0];
  }

  if (lineSegmentPlaneIntersection(sortedCorners[2], sortedCorners[6], plane, t)) {
    intersections[3] = sortedCorners[2] + t * (sortedCorners[6] - sortedCorners[2]);
    intersectionAverage += intersections[3];
    ++count;
  }
  else {
    intersections[3] = intersections[2];
  }

  if (lineSegmentPlaneIntersection(sortedCorners[3], sortedCorners[4], plane, t)) {
    intersections[5] = sortedCorners[3] + t * (sortedCorners[4] - sortedCorners[3]);
    intersectionAverage += intersections[5];
    ++count;
  }
  else {
    intersections[5] = intersections[4];
  }

  intersectionAverage = intersectionAverage / static_cast<T>(count);
  intersections[6] = intersectionAverage;

  return intersections;
}

/**
 * @brief Compute a seven-vertex intersection polygon between an AABB and a plane.
 *
 * This is the public convenience wrapper for box-plane slicing. It first rejects planes that miss
 * the AABB, then computes the sorted corners and final intersection polygon.
 *
 * @param boxCorners The eight corners of the AABB.
 * @param plane Plane equation `(A, B, C, D)` where `Ax + By + Cz + D = 0`.
 * @return The seven-vertex intersection representation, or `std::nullopt` when the plane misses
 * the AABB.
 */
template<typename T>
std::optional<std::array<gvec3<T>, 7> > computeAABBoxPlaneIntersections(
  const std::array<gvec3<T>, 8>& boxCorners,
  const gvec4<T>& plane)
{
  gvec3<T> boxCenter(0, 0, 0);
  gvec3<T> boxMaxCorner(std::numeric_limits<T>::lowest());

  for (const auto& corner : boxCorners) {
    boxCenter += corner;
    boxMaxCorner = glm::max(boxMaxCorner, corner);
  }

  boxCenter /= static_cast<T>(8.0);

  if (!testAABBoxPlaneIntersection(boxCenter, boxMaxCorner, plane)) {
    return std::nullopt;
  }

  std::array<gvec3<T>, 8> sortedCorners;

  if (!computeSortedAABBoxCorners(boxCorners, plane, sortedCorners)) {
    return std::nullopt;
  }

  return computeSliceIntersections(sortedCorners, plane);
}

} // namespace math
