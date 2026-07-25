#pragma once

#include "common/AABB.h"
#include "common/Geometry.h"
#include "common/MathFuncs.h"
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

#include <ranges>

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

class Camera;
class Viewport;

// Note: These math functions mostly come from HistoloZee.
namespace math
{

/**
 * @brief Build an orthonormal basis from a single vector. Use this to create a camera basis with a
 * lookat direction without any priority axes
 *
 * @see "Building an Orthonormal Basis, Revisited" by Tom Duff, James Burgess, Per Christensen,
 * Christophe Hery, Andrew Kensler, Max Liani, and Ryusuke Villemin
 * Journal of Computer Graphics Techniques Vol. 6, No. 1, 2017
 *
 * @param n Unit vector used as the normal of the constructed basis
 * @return Two vectors orthonormal to `n` and to each other
 */
std::pair<glm::vec3, glm::vec3> buildOrthonormalBasis_branchless(const glm::vec3& n);

/**
 * @brief Build an orthonormal basis from a single vector
 * @param n Unit vector used as the normal of the constructed basis
 * @return Two vectors orthonormal to `n` and to each other
 */
std::pair<glm::vec3, glm::vec3> buildOrthonormalBasis(const glm::vec3& n);

/**
 * @brief Convert a signed direction vector to an RGB color triplet
 * @param v Direction vector with components expected near `[-1, 1]`
 * @return RGB color in normalized floating-point form
 */
glm::vec3 convertVecToRGB(const glm::vec3& v);

/**
 * @brief Convert a signed direction vector to an 8-bit RGB color triplet
 * @param v Direction vector with components expected near `[-1, 1]`
 * @return RGB color in unsigned 8-bit form
 */
glm::u8vec3 convertVecToRGB_uint8(const glm::vec3& v);

/**
 * @brief Return indices that order 2D points counterclockwise around their centroid
 * @param points Points to sort
 * @return Indices into `points` in counterclockwise order
 * @throw Propagates exceptions from vector allocation
 */
std::vector<uint32_t> sortCounterclockwise(const std::vector<glm::vec2>& points);

/**
 * @brief Project 3D points into their best-fit local 2D plane
 * @param A 3D points to project
 * @return Projected 2D points
 * @throw Propagates exceptions from vector allocation
 */
std::vector<glm::vec2> project3dPointsToPlane(const std::vector<glm::vec3>& A);

/**
 * @brief Orthogonally project a 3D point onto a plane
 * @param point 3D point to project
 * @param planeEquation Plane equation `(a, b, c, d)` where `ax + by + cz + d = 0`
 * @return Projected 3D point
 */
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
  const std::pair<glm::vec3, glm::vec3>& planeAxes);

/**
 * @brief Add offsets to vertex positions of an object (defined in its own Model space)
 * in order to account for its layering. This function is used when rendering "flat"
 * objects in 2D views
 *
 * @param[in] camera Camera of the view in which th object is rendered
 * @param[in] model_T_world Transformation from World to Model space
 * @param[in] layer Layer of the model
 * @param[in,out] modelPositions Model-space vertex positions that are modified
 * @throw Propagates exceptions from vector access or camera math
 */
void applyLayeringOffsetsToModelPositions(
  const Camera& camera,
  const glm::mat4& model_T_world,
  uint32_t layer,
  std::vector<glm::vec3>& modelPositions);

/**
 * @brief Compute a rotation matrix that rotates one vector onto another
 * @tparam T Floating-point scalar type
 * @param fromVec Source direction
 * @param toVec Target direction
 * @return Rotation matrix from `fromVec` to `toVec`
 */
template<typename T>
gmat4<T> fromToRotation(const gvec3<T>& fromVec, const gvec3<T>& toVec)
{
  gmat4<T> R(T(1.0));

  gvec3<T> v = glm::cross(fromVec, toVec);
  T e = glm::dot(fromVec, toVec);
  T f = (e < T(0.0)) ? -e : e;

  if (f > T(1.0) - glm::epsilon<T>()) {
    // "from" and "to"-vector almost parallel.

    // Vector most nearly orthogonal to "from".
    gvec3<T> x = glm::abs(fromVec);

    if (x[0] < x[1]) {
      if (x[0] < x[2]) {
        x[0] = T(1);
        x[1] = x[2] = T(0);
      }
      else {
        x[2] = T(1);
        x[0] = x[1] = T(0);
      }
    }
    else {
      if (x[1] < x[2]) {
        x[1] = T(1);
        x[0] = x[2] = T(0);
      }
      else {
        x[2] = T(1);
        x[0] = x[1] = T(0);
      }
    }

    gvec3<T> u = x - fromVec;
    gvec3<T> v_temp = x - toVec;

    T c1 = T(2.0) / glm::dot(u, u);
    T c2 = T(2.0) / glm::dot(v_temp, v_temp);
    T c3 = c1 * c2 * glm::dot(u, v_temp);

    for (int row = 0; row < 3; ++row) {
      for (int col = 0; col < 3; ++col) {
        R[col][row] = -c1 * u[row] * u[col] - c2 * v_temp[row] * v_temp[col] + c3 * v_temp[row] * u[col];
      }

      R[row][row] += T(1);
    }
  }
  else {
    // The most common case, unless "from" == "to", or "from" == -"to".

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

/**
 * @brief Compare two 3x3 matrices using GLM epsilon tolerance
 * @tparam T Matrix scalar type
 * @param A First matrix
 * @param B Second matrix
 * @return True when all corresponding elements are equal within tolerance
 */
template<typename T>
bool areMatricesEqual(const gmat3<T>& A, const gmat3<T>& B)
{
  static constexpr float EPS = glm::epsilon<float>();

  if (
    glm::any(glm::epsilonNotEqual(A[0], B[0], EPS)) || glm::any(glm::epsilonNotEqual(A[1], B[1], EPS)) ||
    glm::any(glm::epsilonNotEqual(A[2], B[2], EPS)))
  {
    return false;
  }

  return true;
}

/**
 * @brief Compare two 4x4 matrices using a caller-provided tolerance
 * @tparam T Matrix scalar type
 * @param A First matrix
 * @param B Second matrix
 * @param epsilon Element-wise tolerance
 * @return True when all corresponding elements are equal within `epsilon`
 */
template<typename T>
bool areMatricesEqual(const gmat4<T>& A, const gmat4<T>& B, float epsilon = glm::epsilon<float>())
{
  if (
    glm::any(glm::epsilonNotEqual(A[0], B[0], epsilon)) || glm::any(glm::epsilonNotEqual(A[1], B[1], epsilon)) ||
    glm::any(glm::epsilonNotEqual(A[2], B[2], epsilon)) || glm::any(glm::epsilonNotEqual(A[3], B[3], epsilon)))
  {
    return false;
  }

  return true;
}

/**
 * @brief Check whether a quaternion represents the identity rotation
 * @tparam T Quaternion scalar type
 * @param q Quaternion to test
 * @return True when `q` is identity within GLM epsilon tolerance
 */
template<typename T>
bool isRotationIdentity(const glm::qua<T>& q)
{
  const glm::qua<T> identity(T(1), T(0), T(0), T(0));

  // Quaternions can be negated and still represent the same rotation,
  // so we take the absolute value of the dot product with identity
  const T dot = glm::abs(glm::dot(q, identity));
  return glm::abs(T(1) - dot) < glm::epsilon<T>();
}

/**
 * @brief Compute front-to-back compositing weights for ordered layer opacities
 * @tparam N Number of layers
 * @param layerOpacities Per-layer opacities in draw order
 * @return Per-layer blend weights after attenuation by layers in front
 */
template<size_t N>
std::array<float, N> computeLayerBlendWeights(const std::array<float, N>& layerOpacities)
{
  std::array<float, N> weights = layerOpacities;

  for (size_t i = 0; i < N; ++i) {
    for (size_t j = i + 1; j < N; ++j) {
      weights[i] *= (1.0f - layerOpacities[j]);
    }
  }

  return weights;
}

/**
 * @brief Compute total opacity after front-to-back compositing
 * @tparam N Number of layers
 * @param layerOpacities Per-layer opacities in draw order
 * @return Overall composited opacity
 */
template<size_t N>
float computeOverallOpacity(const std::array<float, N>& layerOpacities)
{
  const std::array<float, N> weights = computeLayerBlendWeights(layerOpacities);

  return std::accumulate(std::begin(weights), std::end(weights), 0.0f);
}

/**
 * @brief Intersect a ray with an axis-aligned bounding box
 * @tparam T Coordinate scalar type
 * @param rayOrig Ray origin
 * @param rayDir Ray direction
 * @param boxMin Minimum AABB corner
 * @param boxMax Maximum AABB corner
 * @return Intersection point when the ray intersects the box; otherwise `std::nullopt`
 */
template<typename T>
std::optional<gvec3<T> >
intersectRayWithAABBox(const gvec3<T>& rayOrig, const gvec3<T>& rayDir, const gvec3<T>& boxMin, const gvec3<T>& boxMax)
{
  const gvec3<T> tmin = (boxMin - rayOrig) / rayDir;
  const gvec3<T> tmax = (boxMax - rayOrig) / rayDir;

  const float minmax = glm::compMin(glm::min(tmin, tmax));
  const float maxmin = glm::compMax(glm::max(tmin, tmax));

  if (minmax >= maxmin) {
    return rayOrig + maxmin * rayDir;
  }

  return std::nullopt;
}

/**
 * @brief Signed distance from 3D point to plane
 * @tparam T Coordinate scalar type
 * @param point 3D point
 * @param plane 3D plane expressed as (a, b, c, d), where ax + by + cz + d = 0
 * @return Positive distance if point is on same side of plane as normal vector;
 * negative if on the other side
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
 * @tparam T Coordinate scalar type
 * @param boxCorners Array of the eight AABB corners
 * @param plane 3D plane expressed as (a, b, c, d), where ax + by + cz + d = 0
 *
 * @return The pair of nearest and farther corners of the AABB with respect to
 * the given plane
 */
template<typename T>
std::tuple<gvec3<T>, T, gvec3<T>, T> computeNearAndFarAABBoxCorners(
  const std::array<gvec3<T>, 8>& boxCorners,
  const gvec4<T>& plane)
{
  T nearCornerDistance = std::numeric_limits<T>::max();
  T farCornerDistance = std::numeric_limits<T>::lowest();

  gvec3<T> nearCorner = boxCorners[0];
  gvec3<T> farCorner = boxCorners[1];

  for (const auto& corner : boxCorners) {
    const T dist = signedDistancePointToPlane(corner, plane);

    if (dist < nearCornerDistance) {
      nearCornerDistance = dist;
      nearCorner = corner;
    }

    if (dist > farCornerDistance) {
      farCornerDistance = dist;
      farCorner = corner;
    }
  }

  return {nearCorner, nearCornerDistance, farCorner, farCornerDistance};
}

/**
 * @brief Compute the left, posterior, and superior directions of the subject in Camera space
 *
 * @param[in] camera_T_world_rotation Rotation matrix from World to Camera space
 * @param[in] world_T_subject_rotation Rotation matrix from Subject to World space
 *
 * @return Left, posterior, and superior directions of the Subject in Camera space
 */
glm::mat3 computeSubjectAxesInCamera(
  const glm::mat3& camera_T_world_rotation,
  const glm::mat3& world_T_subject_rotation);

/**
 * @brief Compute the equation of the view plane in Subject space
 * @param subject_T_world Transform from World space to Subject space
 * @param worldPlaneNormal Plane normal in World space
 * @param worldPlanePoint Point on the plane in World space
 * @return Subject-space plane equation and subject-space point on the plane
 */
std::pair<glm::vec4, glm::vec3> computeSubjectPlaneEquation(
  const glm::mat4 subject_T_world,
  const glm::vec3& worldPlaneNormal,
  const glm::vec3& worldPlanePoint);

/**
 * @brief Compute anatomical label positions for a view from camera and subject transforms
 * @param camera_T_world Transform from World space to Camera space
 * @param world_T_subject Transform from Subject space to World space
 * @return Two anatomical label position descriptors
 */
std::array<AnatomicalLabelPosInfo, 2> computeAnatomicalLabelsForView(
  const glm::mat4& camera_T_world,
  const glm::mat4& world_T_subject);

/**
 * @brief Compute anatomical label positions in miewport coordinates for a rendered view
 * @param miewportViewBounds View bounds in mouse-style y-down viewport coordinates
 * @param windowVP Window viewport
 * @param camera View camera
 * @param world_T_subject Transform from Subject space to World space
 * @param windowClip_T_viewClip Transform from View clip space to Window clip space
 * @param worldCrosshairsPos Crosshairs position in World space
 * @return Two anatomical label position descriptors
 */
std::array<AnatomicalLabelPosInfo, 2> computeAnatomicalLabelPosInfo(
  const FrameBounds& miewportViewBounds,
  const Viewport& windowVP,
  const Camera& camera,
  const glm::mat4& world_T_subject,
  const glm::mat4& windowClip_T_viewClip,
  const glm::vec3& worldCrosshairsPos);

} // namespace math
