#include "logic/camera/Camera.h"
#include "logic/camera/CameraHelpers.h"

#include "common/CoordinateFrame.h"
#include "common/Geometry.h"
#include "common/Viewport.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <glm/ext/matrix_transform.hpp>
#include <glm/gtc/constants.hpp>

#include <array>
#include <cmath>
#include <limits>

namespace
{

void checkVec3(const glm::vec3& actual, const glm::vec3& expected)
{
  CHECK(actual.x == Catch::Approx(expected.x));
  CHECK(actual.y == Catch::Approx(expected.y));
  CHECK(actual.z == Catch::Approx(expected.z));
}

std::array<glm::vec3, 8> boxCorners(const glm::vec3& center, const glm::vec3& size)
{
  const glm::vec3 low = center - 0.5f * size;
  const glm::vec3 high = center + 0.5f * size;
  return {
    {{low.x, low.y, low.z},
     {high.x, low.y, low.z},
     {low.x, high.y, low.z},
     {high.x, high.y, low.z},
     {low.x, low.y, high.z},
     {high.x, low.y, high.z},
     {low.x, high.y, high.z},
     {high.x, high.y, high.z}}};
}

} // namespace

TEST_CASE("default camera framing adds five percent per side", "[camera][recenter]")
{
  checkVec3(helper::defaultViewFramingSize(glm::vec3{100.0f, 50.0f, 10.0f}), glm::vec3{110.0f, 55.0f, 11.0f});
}

TEST_CASE("camera framing clears a top-left control overlay only when content intersects it", "[camera][recenter]")
{
  Camera camera(ProjectionType::Orthographic);
  const AABB<float> squareBox{glm::vec3{-50.0f, -50.0f, -1.0f}, glm::vec3{50.0f, 50.0f, 1.0f}};
  helper::positionCameraForWorldTargetAndFov(
    camera,
    helper::defaultViewFramingSize(math::computeAABBoxSize(squareBox)),
    math::computeAABBoxCenter(squareBox));

  const float intersectingScale =
    helper::viewFramingScaleForOverlay(camera, squareBox, glm::vec2{1000.0f}, glm::vec4{0.0f, 0.0f, 300.0f, 80.0f});
  CHECK(intersectingScale > 1.0f);

  const AABB<float> narrowBox{glm::vec3{-10.0f, -50.0f, -1.0f}, glm::vec3{10.0f, 50.0f, 1.0f}};
  helper::positionCameraForWorldTargetAndFov(
    camera,
    helper::defaultViewFramingSize(math::computeAABBoxSize(narrowBox)),
    math::computeAABBoxCenter(narrowBox));
  CHECK(
    helper::viewFramingScaleForOverlay(camera, narrowBox, glm::vec2{1000.0f}, glm::vec4{0.0f, 0.0f, 300.0f, 80.0f}) ==
    Catch::Approx(1.0f));
}

TEST_CASE("overlay-safe framing can retain its established crosshairs orientation", "[camera][recenter][crosshairs]")
{
  CoordinateFrame crosshairs;
  Camera camera{ProjectionType::Orthographic, [&crosshairs]() {
                  return crosshairs;
                }};
  camera.setAspectRatio(1.0f);
  camera.setDefaultFov(glm::vec2{120.0f, 80.0f});

  const AABB<float> worldBox{glm::vec3{-50.0f, -30.0f, -5.0f}, glm::vec3{50.0f, 30.0f, 5.0f}};
  const glm::vec2 viewSize{1000.0f, 1000.0f};
  const glm::vec4 overlayBounds{0.0f, 0.0f, 350.0f, 140.0f};
  const glm::mat4 establishedCamera_T_world = camera.camera_T_world();
  const float establishedScale = helper::viewFramingScaleForOverlay(
    camera.clip_T_camera() * establishedCamera_T_world,
    worldBox,
    viewSize,
    overlayBounds);

  crosshairs.setFrameToWorldRotation(47.0f, glm::vec3{0.0f, 0.0f, 1.0f});

  CHECK(
    helper::viewFramingScaleForOverlay(
      camera.clip_T_camera() * establishedCamera_T_world,
      worldBox,
      viewSize,
      overlayBounds) == Catch::Approx(establishedScale));
  CHECK(
    helper::viewFramingScaleForOverlay(camera, worldBox, viewSize, overlayBounds) != Catch::Approx(establishedScale));
}

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

TEST_CASE("2D crosshairs rotation helpers return unit rotations and handle a centered drag", "[camera][crosshairs]")
{
  Camera camera(ProjectionType::Orthographic);
  const glm::vec2 center{0.0f};

  const glm::quat rotation =
    helper::rotation2dInCameraPlane(camera, glm::vec2{1.0f, 0.0f}, glm::vec2{0.0f, 1.0f}, center);
  CHECK(glm::length(rotation) == Catch::Approx(1.0f).margin(1.0e-6f));

  const glm::quat snapped = helper::rotation2dInCameraPlaneWithSnapping(
    camera,
    glm::vec2{1.0f, 0.0f},
    glm::vec2{std::sqrt(0.5f), std::sqrt(0.5f)},
    15.0f,
    7.5f,
    center);
  CHECK(glm::length(snapped) == Catch::Approx(1.0f).margin(1.0e-6f));

  const glm::quat centeredStart =
    helper::rotation2dInCameraPlaneWithSnapping(camera, center, glm::vec2{0.0f, 1.0f}, 15.0f, 7.5f, center);
  CHECK(centeredStart == glm::quat{1.0f, 0.0f, 0.0f, 0.0f});
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

TEST_CASE("orthographic pixel scale stays fixed while the linked frame rotates", "[camera][scale-bar][crosshairs]")
{
  CoordinateFrame linkedFrame{glm::vec3{1'000'000.0f, -2'000'000.0f, 3'000'000.0f}, glm::quat{1.0f, 0.0f, 0.0f, 0.0f}};
  Camera camera{ProjectionType::Orthographic, [&linkedFrame]() {
                  return linkedFrame;
                }};
  camera.setAspectRatio(16.0f / 9.0f);
  camera.setDefaultFov(glm::vec2{240.0f, 120.0f});
  camera.setZoom(1.5f);

  const Viewport windowViewport{0.0f, 0.0f, 1600.0f, 900.0f};
  const glm::mat4 viewClip_T_windowClip = glm::scale(glm::mat4{1.0f}, glm::vec3{2.0f, 1.0f, 1.0f});
  const glm::vec2 expected{0.2f, 0.1f};

  const auto checkScale = [&]() {
    const glm::vec2 scale = helper::worldPixelSize(windowViewport, camera, viewClip_T_windowClip);
    CHECK(scale.x == Catch::Approx(expected.x).margin(1.0e-6f));
    CHECK(scale.y == Catch::Approx(expected.y).margin(1.0e-6f));
  };

  checkScale();
  for (const float angleDegrees : {17.0f, 43.0f, 89.0f, 137.0f, 221.0f, 319.0f}) {
    linkedFrame.setFrameToWorldRotation(angleDegrees, glm::normalize(glm::vec3{1.0f, 2.0f, 3.0f}));
    checkScale();
  }
}

TEST_CASE("orthographic camera positioning supports submillimeter scenes", "[camera][2d][recenter]")
{
  Camera camera(ProjectionType::Orthographic);
  const glm::vec3 worldTarget{0.01f, -0.02f, 0.03f};
  const glm::vec3 worldFov{0.01f, 0.02f, 0.005f};

  helper::positionCameraForWorldTargetAndFov(camera, worldFov, worldTarget);

  const glm::vec3 targetNdc = helper::ndc_T_world(camera, worldTarget);
  CHECK(camera.nearDistance() < glm::distance(helper::worldOrigin(camera), worldTarget));
  CHECK(targetNdc.x == Catch::Approx(0.0f));
  CHECK(targetNdc.y == Catch::Approx(0.0f));
  CHECK(targetNdc.z > -1.0f);
  CHECK(targetNdc.z < 1.0f);
}

TEST_CASE("perspective camera positioning accounts for portrait aspect ratios", "[camera][recenter]")
{
  Camera camera(ProjectionType::Perspective);
  camera.setAspectRatio(0.35f);
  const glm::vec3 worldTarget{3.0f, 4.0f, 5.0f};
  const glm::vec3 worldSize{40.0f, 20.0f, 10.0f};

  helper::positionCameraForWorldTargetAndFov(camera, worldSize, worldTarget);

  for (const glm::vec3& corner : boxCorners(worldTarget, worldSize)) {
    const glm::vec3 ndc = helper::ndc_T_world(camera, corner);
    CHECK(std::abs(ndc.x) <= 1.0f);
    CHECK(std::abs(ndc.y) <= 1.0f);
    CHECK(ndc.z >= -1.0f);
    CHECK(ndc.z <= 1.0f);
  }
}

TEST_CASE("camera positioning ignores non-finite targets", "[camera][2d][recenter]")
{
  Camera camera(ProjectionType::Orthographic);
  const glm::mat4 transform = camera.camera_T_anatomy();

  helper::positionCameraForWorldTargetAndFov(
    camera,
    glm::vec3{10.0f},
    glm::vec3{std::numeric_limits<float>::quiet_NaN()});

  CHECK(camera.camera_T_anatomy() == transform);
}
