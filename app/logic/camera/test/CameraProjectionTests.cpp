#include "logic/camera/Camera.h"

#include "logic/camera/CameraHelpers.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <glm/ext/matrix_transform.hpp>
#include <glm/glm.hpp>

#include <cmath>
#include <limits>

TEST_CASE("perspective and orthographic projections map their clip planes to NDC", "[camera][projection]")
{
  for (const ProjectionType type : {ProjectionType::Perspective, ProjectionType::Orthographic}) {
    Camera camera(type);
    camera.setClipDistances(2.0f, 20.0f);

    CHECK(helper::ndc_T_camera(camera, glm::vec3{0.0f, 0.0f, -2.0f}).z == Catch::Approx(-1.0f));
    CHECK(helper::ndc_T_camera(camera, glm::vec3{0.0f, 0.0f, -20.0f}).z == Catch::Approx(1.0f));
  }
}

TEST_CASE("projection parameters reject non-finite and inconsistent values", "[camera][projection]")
{
  Camera camera(ProjectionType::Perspective);
  camera.setAspectRatio(1.5f);
  camera.setDefaultFov(glm::vec2{12.0f, 8.0f});
  camera.setClipDistances(2.0f, 20.0f);
  camera.setZoom(2.0f);

  const float nan = std::numeric_limits<float>::quiet_NaN();
  const float infinity = std::numeric_limits<float>::infinity();
  camera.setAspectRatio(infinity);
  camera.setDefaultFov(glm::vec2{nan, 10.0f});
  camera.setClipDistances(30.0f, 20.0f);
  camera.setNearDistance(nan);
  camera.setFarDistance(infinity);
  camera.setZoom(nan);

  CHECK(camera.aspectRatio() == Catch::Approx(1.5f));
  CHECK(camera.projection()->defaultFov() == (glm::vec2{12.0f, 8.0f}));
  CHECK(camera.nearDistance() == Catch::Approx(2.0f));
  CHECK(camera.farDistance() == Catch::Approx(20.0f));
  CHECK(camera.getZoom() == Catch::Approx(2.0f));
}

TEST_CASE("camera copies preserve extreme projection parameters atomically", "[camera][projection]")
{
  Camera source(ProjectionType::Perspective);
  source.setAspectRatio(0.75f);
  source.setDefaultFov(glm::vec2{40.0f, 30.0f});
  source.setClipDistances(2000.0f, 5000.0f);
  source.setZoom(25.0f);

  const Camera copy = source;

  CHECK(copy.projection()->type() == ProjectionType::Perspective);
  CHECK(copy.aspectRatio() == Catch::Approx(0.75f));
  CHECK(copy.projection()->defaultFov() == (glm::vec2{40.0f, 30.0f}));
  CHECK(copy.nearDistance() == Catch::Approx(2000.0f));
  CHECK(copy.farDistance() == Catch::Approx(5000.0f));
  CHECK(copy.getZoom() == Catch::Approx(25.0f));

  Camera assigned(ProjectionType::Orthographic);
  assigned = source;
  CHECK(assigned.projection()->type() == ProjectionType::Perspective);
  CHECK(assigned.nearDistance() == Catch::Approx(2000.0f));
  CHECK(assigned.farDistance() == Catch::Approx(5000.0f));
}

TEST_CASE("camera rejects volume-preserving transforms that are not rigid", "[camera][projection]")
{
  Camera camera(ProjectionType::Perspective);
  const glm::mat4 nonRigid = glm::scale(glm::mat4{1.0f}, glm::vec3{2.0f, 0.5f, 1.0f});

  camera.set_camera_T_anatomy(nonRigid);

  CHECK(camera.camera_T_anatomy() == glm::mat4{1.0f});
}

TEST_CASE("zoom clamping does not move the camera when magnification cannot change", "[camera][projection]")
{
  Camera camera(ProjectionType::Orthographic);
  camera.setZoom(1000.0f);
  const glm::vec3 eye = helper::worldOrigin(camera);

  helper::zoomNdc(camera, 2.0f, glm::vec2{0.5f, -0.25f});

  CHECK(camera.getZoom() == Catch::Approx(1000.0f));
  CHECK(helper::worldOrigin(camera) == eye);
}

TEST_CASE("high zoom retains usable rotate-about-eye sensitivity", "[camera][projection][interaction]")
{
  Camera camera(ProjectionType::Orthographic);
  camera.setZoom(1000.0f);
  const glm::vec3 front = helper::worldDirection(camera, Directions::View::Front);

  helper::rotateAboutCameraOrigin(camera, glm::vec2{0.0f}, glm::vec2{0.1f, 0.0f});

  CHECK(glm::dot(front, helper::worldDirection(camera, Directions::View::Front)) < 0.99999f);
}

TEST_CASE("perspective framing uses the projection half-angle", "[camera][projection]")
{
  Camera camera(ProjectionType::Perspective);

  const auto [pullback, farDistance] = helper::computePullbackAndFarDistances(camera, glm::vec3{10.0f, 0.0f, 0.0f});

  CHECK(pullback == Catch::Approx(5.0f / std::tan(0.5f * camera.angle())));
  CHECK(farDistance > pullback);
}
