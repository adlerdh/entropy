#include "logic/camera/Camera.h"
#include "logic/camera/CameraHelpers.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <glm/ext/matrix_transform.hpp>
#include <glm/gtc/constants.hpp>

namespace
{

void checkVec3(const glm::vec3& actual, const glm::vec3& expected)
{
  CHECK(actual.x == Catch::Approx(expected.x));
  CHECK(actual.y == Catch::Approx(expected.y));
  CHECK(actual.z == Catch::Approx(expected.z));
}

} // namespace

TEST_CASE("camera reset helpers restore zoom and view transform", "[camera][recenter]")
{
  Camera camera(ProjectionType::Orthographic);
  camera.setZoom(3.0f);
  helper::applyViewTransformation(
    camera,
    glm::rotate(glm::mat4{1.0f}, glm::half_pi<float>(), glm::vec3{0.0f, 0.0f, 1.0f}));

  helper::resetZoom(camera);
  helper::resetViewTransformation(camera);

  CHECK(camera.getZoom() == Catch::Approx(1.0f));
  CHECK(camera.camera_T_anatomy() == glm::mat4{1.0f});
}

TEST_CASE("camera positioning centers the target in clip space", "[camera][recenter]")
{
  Camera camera(ProjectionType::Orthographic);
  const glm::vec3 worldTarget{3.0f, 4.0f, 5.0f};
  const glm::vec3 worldFov{20.0f, 10.0f, 4.0f};

  helper::positionCameraForWorldTargetAndFov(camera, worldFov, worldTarget);

  const glm::vec3 targetNdc = helper::ndc_T_world(camera, worldTarget);

  CHECK(targetNdc.x == Catch::Approx(0.0f));
  CHECK(targetNdc.y == Catch::Approx(0.0f));
  CHECK(camera.projection()->defaultFov().x == Catch::Approx(20.0f));
  CHECK(camera.projection()->defaultFov().y == Catch::Approx(10.0f));
  CHECK(camera.farDistance() > 20.0f);
}

TEST_CASE("orthographic fit uses camera-plane extent instead of full 3D maximum extent", "[camera][recenter]")
{
  Camera camera(ProjectionType::Orthographic);
  const glm::vec3 worldTarget{0.0f};
  const glm::vec3 worldFov{10.0f, 20.0f, 300.0f};

  helper::positionCameraForWorldTargetAndFov(camera, worldFov, worldTarget);

  CHECK(camera.projection()->defaultFov().x == Catch::Approx(10.0f));
  CHECK(camera.projection()->defaultFov().y == Catch::Approx(20.0f));
}

TEST_CASE("camera positioning preserves view direction while moving the origin", "[camera][recenter]")
{
  Camera camera(ProjectionType::Orthographic);
  const glm::vec3 initialFront = helper::worldDirection(camera, Directions::View::Front);
  const glm::vec3 worldTarget{1.0f, 2.0f, 3.0f};

  helper::positionCameraForWorldTargetAndFov(camera, glm::vec3{8.0f}, worldTarget);

  checkVec3(helper::worldDirection(camera, Directions::View::Front), initialFront);
  CHECK(glm::distance(helper::worldOrigin(camera), worldTarget) > 0.0f);
}
