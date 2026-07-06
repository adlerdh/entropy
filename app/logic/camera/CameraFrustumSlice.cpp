#include "logic/camera/CameraFrustumSlice.h"

#include "logic/camera/CameraHelpers.h"

#include <glm/geometric.hpp>

#include <algorithm>
#include <array>
#include <cmath>

namespace
{

using Face = camera3d::FrustumSliceSegment::Face;

constexpr float k_planeTolerance = 1.0e-5f;
constexpr float k_duplicateTolerance = 1.0e-4f;

struct FaceCorners
{
  Face face;
  std::array<int, 4> corners;
};

constexpr std::array<FaceCorners, 6> k_faces{{
  {Face::Near, {0, 1, 2, 3}},
  {Face::Far, {4, 5, 6, 7}},
  {Face::Right, {0, 3, 7, 4}},
  {Face::Top, {0, 1, 5, 4}},
  {Face::Left, {1, 2, 6, 5}},
  {Face::Bottom, {2, 3, 7, 6}},
}};

bool isFiniteVec3(const glm::vec3& v)
{
  return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}

void pushUnique(std::vector<glm::vec3>& points, const glm::vec3& point)
{
  if (!isFiniteVec3(point)) {
    return;
  }

  const auto duplicate = std::find_if(points.begin(), points.end(), [&point](const glm::vec3& existing) {
    return glm::length(existing - point) < k_duplicateTolerance;
  });
  if (duplicate == points.end()) {
    points.push_back(point);
  }
}

std::vector<glm::vec3> facePlaneIntersections(
  const std::array<glm::vec3, 8>& corners,
  const FaceCorners& face,
  const glm::vec3& planePoint,
  const glm::vec3& planeNormal)
{
  std::vector<glm::vec3> points;
  for (std::size_t i = 0; i < face.corners.size(); ++i) {
    const glm::vec3 a = corners[face.corners[i]];
    const glm::vec3 b = corners[face.corners[(i + 1) % face.corners.size()]];
    const float da = glm::dot(a - planePoint, planeNormal);
    const float db = glm::dot(b - planePoint, planeNormal);

    if (std::abs(da) <= k_planeTolerance) {
      pushUnique(points, a);
    }
    if (std::abs(db) <= k_planeTolerance) {
      pushUnique(points, b);
    }
    if ((da < -k_planeTolerance && db > k_planeTolerance) || (da > k_planeTolerance && db < -k_planeTolerance)) {
      const float t = da / (da - db);
      pushUnique(points, a + t * (b - a));
    }
  }

  return points;
}

} // namespace

namespace camera3d
{

FrustumSliceOverlay frustumSliceOverlay(const Camera& camera, const glm::vec3& planePoint, const glm::vec3& planeNormal)
{
  FrustumSliceOverlay overlay{helper::worldOrigin(camera), {}};

  if (glm::length(planeNormal) <= 0.0f) {
    return overlay;
  }

  const glm::vec3 normal = glm::normalize(planeNormal);
  const std::array<glm::vec3, 8> corners = helper::worldFrustumCorners(camera);

  for (const FaceCorners& face : k_faces) {
    std::vector<glm::vec3> points = facePlaneIntersections(corners, face, planePoint, normal);
    if (points.size() < 2) {
      continue;
    }

    // A convex quad intersects a plane in at most one segment. If numerical tolerance left more
    // than two unique points, keep the most distant pair to cover the whole intersection.
    std::array<glm::vec3, 2> segment{points[0], points[1]};
    float maxDistance = glm::length(segment[1] - segment[0]);
    for (std::size_t i = 0; i < points.size(); ++i) {
      for (std::size_t j = i + 1; j < points.size(); ++j) {
        const float distance = glm::length(points[j] - points[i]);
        if (distance > maxDistance) {
          segment = {points[i], points[j]};
          maxDistance = distance;
        }
      }
    }

    if (maxDistance > k_duplicateTolerance) {
      overlay.segments.push_back({.face = face.face, .a = segment[0], .b = segment[1]});
    }
  }

  return overlay;
}

} // namespace camera3d
