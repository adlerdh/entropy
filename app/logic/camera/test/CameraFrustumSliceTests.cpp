#include "logic/camera/CameraFrustumSlice.h"

#include "logic/camera/Camera.h"
#include "logic/camera/Camera3DControls.h"
#include "logic/camera/CameraHelpers.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <glm/glm.hpp>

#include <algorithm>

namespace
{

using Face = camera3d::FrustumSliceSegment::Face;

camera3d::SceneFrame unitScene()
{
  return camera3d::SceneFrame{
    .m_center = glm::vec3{0.0f},
    .m_size = glm::vec3{10.0f, 10.0f, 10.0f},
    .m_voxelDiagonal = 1.0f};
}

std::size_t countFace(const camera3d::FrustumSliceOverlay& overlay, Face face)
{
  return static_cast<std::size_t>(
    std::count_if(overlay.segments.begin(), overlay.segments.end(), [face](const auto& s) { return s.face == face; }));
}

void checkPointOnPlane(const glm::vec3& point, const glm::vec3& planePoint, const glm::vec3& planeNormal)
{
  CHECK(glm::dot(point - planePoint, planeNormal) == Catch::Approx(0.0f).margin(1.0e-4f));
}

float frontDepth(const Camera& camera, const glm::vec3& point)
{
  const glm::vec3 eye = helper::worldOrigin(camera);
  const glm::vec3 front = helper::worldDirection(camera, Directions::View::Front);
  return glm::dot(point - eye, front);
}

} // namespace

TEST_CASE("3D camera frustum corners match camera near and far distances", "[camera][frustum]")
{
  Camera camera(ProjectionType::Perspective);
  camera3d::State state;
  camera3d::setDefaultCoronalPose(camera, state, unitScene());

  const auto corners = helper::worldFrustumCorners(camera);
  for (int i = 0; i < 4; ++i) {
    CHECK(
      frontDepth(camera, corners[static_cast<std::size_t>(i)]) ==
      Catch::Approx(camera.nearDistance()).epsilon(1.0e-4f));
    CHECK(
      frontDepth(camera, corners[static_cast<std::size_t>(i + 4)]) ==
      Catch::Approx(camera.farDistance()).epsilon(1.0e-4f));
  }
}

TEST_CASE("frustum slice overlay handles a plane parallel to near and far faces", "[camera][frustum]")
{
  Camera camera(ProjectionType::Perspective);
  camera3d::State state;
  camera3d::setDefaultCoronalPose(camera, state, unitScene());

  const glm::vec3 planePoint{0.0f};
  const glm::vec3 planeNormal{0.0f, 1.0f, 0.0f};
  const camera3d::FrustumSliceOverlay overlay = camera3d::frustumSliceOverlay(camera, planePoint, planeNormal);

  CHECK(overlay.segments.size() == 4);
  CHECK(countFace(overlay, Face::Near) == 0);
  CHECK(countFace(overlay, Face::Far) == 0);
  CHECK(countFace(overlay, Face::Left) == 1);
  CHECK(countFace(overlay, Face::Right) == 1);
  CHECK(countFace(overlay, Face::Top) == 1);
  CHECK(countFace(overlay, Face::Bottom) == 1);

  for (const auto& segment : overlay.segments) {
    checkPointOnPlane(segment.a, planePoint, planeNormal);
    checkPointOnPlane(segment.b, planePoint, planeNormal);
    CHECK(glm::length(segment.b - segment.a) > 0.0f);
  }
}

TEST_CASE("frustum slice overlay draws near and far plane intersections", "[camera][frustum]")
{
  Camera camera(ProjectionType::Perspective);
  camera3d::State state;
  camera3d::setDefaultCoronalPose(camera, state, unitScene());

  const glm::vec3 planePoint{0.0f};
  const glm::vec3 planeNormal{1.0f, 0.0f, 0.0f};
  const camera3d::FrustumSliceOverlay overlay = camera3d::frustumSliceOverlay(camera, planePoint, planeNormal);

  CHECK(countFace(overlay, Face::Near) == 1);
  CHECK(countFace(overlay, Face::Far) == 1);
  for (const auto& segment : overlay.segments) {
    checkPointOnPlane(segment.a, planePoint, planeNormal);
    checkPointOnPlane(segment.b, planePoint, planeNormal);
    if (Face::Near == segment.face) {
      CHECK(frontDepth(camera, segment.a) == Catch::Approx(camera.nearDistance()).epsilon(1.0e-4f));
      CHECK(frontDepth(camera, segment.b) == Catch::Approx(camera.nearDistance()).epsilon(1.0e-4f));
    }
    if (Face::Far == segment.face) {
      CHECK(frontDepth(camera, segment.a) == Catch::Approx(camera.farDistance()).epsilon(1.0e-4f));
      CHECK(frontDepth(camera, segment.b) == Catch::Approx(camera.farDistance()).epsilon(1.0e-4f));
    }
  }
}

TEST_CASE("frustum slice overlay returns only the eye when the plane misses the frustum", "[camera][frustum]")
{
  Camera camera(ProjectionType::Perspective);
  camera3d::State state;
  camera3d::setDefaultCoronalPose(camera, state, unitScene());

  const camera3d::FrustumSliceOverlay overlay =
    camera3d::frustumSliceOverlay(camera, glm::vec3{0.0f, 10000.0f, 0.0f}, glm::vec3{0.0f, 1.0f, 0.0f});

  CHECK(overlay.segments.empty());
}

TEST_CASE("frustum slice overlay supports orthographic frusta", "[camera][frustum]")
{
  Camera camera(ProjectionType::Orthographic);
  camera3d::State state;
  camera3d::setDefaultCoronalPose(camera, state, unitScene());
  camera3d::setProjection(camera, state, ProjectionType::Orthographic);

  const glm::vec3 planePoint{0.0f};
  const glm::vec3 planeNormal{1.0f, 0.0f, 0.0f};
  const camera3d::FrustumSliceOverlay overlay = camera3d::frustumSliceOverlay(camera, planePoint, planeNormal);

  CHECK_FALSE(overlay.segments.empty());
  for (const auto& segment : overlay.segments) {
    checkPointOnPlane(segment.a, planePoint, planeNormal);
    checkPointOnPlane(segment.b, planePoint, planeNormal);
  }
}
