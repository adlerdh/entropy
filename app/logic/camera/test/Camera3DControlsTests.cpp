#include "logic/camera/Camera3DControls.h"

#include "common/DirectionMaps.h"
#include "logic/camera/Camera.h"
#include "logic/camera/CameraHelpers.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <glm/glm.hpp>

namespace
{

void checkVec3Approx(const glm::vec3& actual, const glm::vec3& expected, float margin = 1.0e-4f)
{
  CHECK(actual.x == Catch::Approx(expected.x).margin(margin));
  CHECK(actual.y == Catch::Approx(expected.y).margin(margin));
  CHECK(actual.z == Catch::Approx(expected.z).margin(margin));
}

camera3d::SceneFrame testScene()
{
  return camera3d::SceneFrame{
    .m_center = glm::vec3{1.0f, 2.0f, 3.0f},
    .m_size = glm::vec3{10.0f, 20.0f, 30.0f},
    .m_voxelDiagonal = 2.0f};
}

} // namespace

TEST_CASE("3D camera default pose is coronal and outside the scene", "[camera][3d]")
{
  Camera camera(ProjectionType::Perspective);
  camera3d::State state;
  const camera3d::SceneFrame scene = testScene();

  camera3d::setDefaultCoronalPose(camera, state, scene);

  checkVec3Approx(
    helper::worldDirection(camera, Directions::View::Right),
    Directions::get(Directions::Cartesian::PosX));
  checkVec3Approx(helper::worldDirection(camera, Directions::View::Up), Directions::get(Directions::Cartesian::PosZ));
  checkVec3Approx(
    helper::worldDirection(camera, Directions::View::Front),
    Directions::get(Directions::Cartesian::PosY));
  CHECK(
    glm::distance(helper::worldOrigin(camera), scene.m_center) ==
    Catch::Approx(0.75f * camera3d::sceneDiagonal(scene.m_size)));
  CHECK(camera.nearDistance() > 0.0f);
  CHECK(camera.farDistance() > camera.nearDistance());
}

TEST_CASE("3D camera scene metrics are finite for degenerate scenes", "[camera][3d]")
{
  const camera3d::SceneMetrics metrics = camera3d::sceneMetrics(camera3d::SceneFrame{.m_size = glm::vec3{0.0f}});

  CHECK(metrics.m_diagonal >= 1.0e-4f);
  CHECK(metrics.m_defaultOrbitDistance >= 1.0e-4f);
  CHECK(metrics.m_minTargetDistance >= 1.0e-4f);
  CHECK(metrics.m_minPanDistance >= 1.0e-4f);
  CHECK(metrics.m_scrollDistance >= 1.0e-4f);
  CHECK(metrics.m_defaultFov.x >= 1.0e-4f);
  CHECK(metrics.m_defaultFov.y >= 1.0e-4f);
}

TEST_CASE("3D camera scene metrics preserve sub-millimeter scene sizes", "[camera][3d]")
{
  const glm::vec3 sceneSize{0.01f, 0.02f, 0.03f};
  const camera3d::SceneMetrics metrics =
    camera3d::sceneMetrics(camera3d::SceneFrame{.m_size = sceneSize, .m_voxelDiagonal = 0.005f});
  const float diagonal = glm::length(sceneSize);

  CHECK(metrics.m_diagonal == Catch::Approx(diagonal));
  CHECK(metrics.m_voxelDiagonal == Catch::Approx(0.005f));
  CHECK(metrics.m_defaultOrbitDistance == Catch::Approx(0.75f * diagonal));
  CHECK(metrics.m_minPanDistance == Catch::Approx(0.25f * diagonal));
  CHECK(metrics.m_defaultFov.x == Catch::Approx(0.03f));
  CHECK(metrics.m_defaultFov.y == Catch::Approx(0.03f));
}

TEST_CASE("3D perspective scroll distance is a small fraction of scene size", "[camera][3d]")
{
  const camera3d::SceneMetrics metrics =
    camera3d::sceneMetrics(camera3d::SceneFrame{.m_size = glm::vec3{10.0f, 20.0f, 30.0f}});

  CHECK(metrics.m_scrollDistance == Catch::Approx(0.01f * metrics.m_diagonal));
}

TEST_CASE("3D clipping planes use visible scene radius and voxel scale outside the scene", "[camera][3d]")
{
  Camera camera(ProjectionType::Perspective);
  const camera3d::SceneFrame scene{
    .m_center = glm::vec3{0.0f},
    .m_size = glm::vec3{20.0f, 0.0f, 0.0f},
    .m_voxelDiagonal = 2.0f};

  camera3d::configureClipPlanes(camera, scene, 30.0f);

  CHECK(camera.nearDistance() == Catch::Approx(18.0f));
  CHECK(camera.farDistance() == Catch::Approx(42.0f));
}

TEST_CASE("3D clipping planes support cameras inside the visible scene", "[camera][3d]")
{
  Camera camera(ProjectionType::Perspective);
  const camera3d::SceneFrame scene{
    .m_center = glm::vec3{0.0f},
    .m_size = glm::vec3{20.0f, 0.0f, 0.0f},
    .m_voxelDiagonal = 2.0f};

  camera3d::configureClipPlanes(camera, scene, 4.0f);

  CHECK(camera.nearDistance() == Catch::Approx(1.0f));
  CHECK(camera.farDistance() == Catch::Approx(21.0f));
}

TEST_CASE("3D camera default coronal pose is a pure helper", "[camera][3d]")
{
  const camera3d::SceneFrame scene = testScene();
  const camera3d::SceneMetrics metrics = camera3d::sceneMetrics(scene);
  const camera3d::Pose pose = camera3d::defaultCoronalPose(scene);

  checkVec3Approx(pose.m_eye, scene.m_center + metrics.m_defaultOrbitDistance * glm::vec3{0.0f, -1.0f, 0.0f});
  checkVec3Approx(pose.m_right, glm::vec3{1.0f, 0.0f, 0.0f});
  checkVec3Approx(pose.m_up, glm::vec3{0.0f, 0.0f, 1.0f});
  checkVec3Approx(pose.m_back, glm::vec3{0.0f, -1.0f, 0.0f});
}

TEST_CASE("3D camera scene updates do not recenter a manually moved camera", "[camera][3d]")
{
  Camera camera(ProjectionType::Perspective);
  camera3d::State state;
  camera3d::Controller controller{camera, state};
  controller.initializeDefaultPose(testScene());

  controller.pan(glm::vec2{0.0f}, glm::vec2{0.15f, 0.0f});
  const glm::vec3 eye = helper::worldOrigin(camera);
  const glm::vec3 target = state.m_orbitTarget;
  REQUIRE(state.m_userMovedCamera);

  controller.updateScene(camera3d::SceneFrame{.m_center = glm::vec3{100.0f}, .m_size = glm::vec3{50.0f}});

  checkVec3Approx(helper::worldOrigin(camera), eye);
  checkVec3Approx(state.m_orbitTarget, target);
  CHECK(state.m_userMovedCamera);
}

TEST_CASE("3D orbit preserves distance to target", "[camera][3d]")
{
  Camera camera(ProjectionType::Perspective);
  camera3d::State state;
  const camera3d::SceneFrame scene = testScene();
  camera3d::setDefaultCoronalPose(camera, state, scene);

  const float before = glm::distance(helper::worldOrigin(camera), state.m_orbitTarget);
  camera3d::orbit(camera, state, glm::vec2{0.0f}, glm::vec2{0.2f, 0.1f});

  CHECK(glm::distance(helper::worldOrigin(camera), state.m_orbitTarget) == Catch::Approx(before).epsilon(1.0e-4f));
  CHECK(state.m_userMovedCamera);
}

TEST_CASE("3D horizontal orbit rotates about the selected center", "[camera][3d]")
{
  Camera camera(ProjectionType::Perspective);
  camera3d::State state;
  camera3d::setDefaultCoronalPose(camera, state, testScene());

  const float distance = glm::distance(helper::worldOrigin(camera), state.m_orbitTarget);
  const glm::vec3 before = helper::worldOrigin(camera) - state.m_orbitTarget;
  camera3d::orbit(camera, state, glm::vec2{0.0f}, glm::vec2{0.2f, 0.0f});
  const glm::vec3 after = helper::worldOrigin(camera) - state.m_orbitTarget;

  CHECK(glm::distance(helper::worldOrigin(camera), state.m_orbitTarget) == Catch::Approx(distance).epsilon(1.0e-4f));
  CHECK(glm::distance(after, before) > 0.0f);
}

TEST_CASE("3D vertical orbit rotates about the selected center", "[camera][3d]")
{
  Camera camera(ProjectionType::Perspective);
  camera3d::State state;
  camera3d::setDefaultCoronalPose(camera, state, testScene());

  const float distance = glm::distance(helper::worldOrigin(camera), state.m_orbitTarget);
  const glm::vec3 before = helper::worldOrigin(camera) - state.m_orbitTarget;
  camera3d::orbit(camera, state, glm::vec2{0.0f}, glm::vec2{0.0f, 0.2f});
  const glm::vec3 after = helper::worldOrigin(camera) - state.m_orbitTarget;

  CHECK(glm::distance(helper::worldOrigin(camera), state.m_orbitTarget) == Catch::Approx(distance).epsilon(1.0e-4f));
  CHECK(glm::distance(after, before) > 0.0f);
}

TEST_CASE("3D eye rotation preserves camera origin", "[camera][3d]")
{
  Camera camera(ProjectionType::Perspective);
  camera3d::State state;
  camera3d::setDefaultCoronalPose(camera, state, testScene());

  const glm::vec3 eye = helper::worldOrigin(camera);
  camera3d::rotateAboutEye(camera, state, glm::vec2{0.0f}, glm::vec2{0.15f, -0.05f});

  checkVec3Approx(helper::worldOrigin(camera), eye);
}

TEST_CASE("3D pan moves orbit target", "[camera][3d]")
{
  Camera camera(ProjectionType::Perspective);
  camera3d::State state;
  camera3d::setDefaultCoronalPose(camera, state, testScene());

  const glm::vec3 target = state.m_orbitTarget;
  camera3d::pan(camera, state, glm::vec2{0.0f}, glm::vec2{0.1f, -0.2f});

  CHECK(glm::distance(state.m_orbitTarget, target) > 0.0f);
}

TEST_CASE("3D scene-AABB pan uses a picked scene plane", "[camera][3d]")
{
  Camera camera(ProjectionType::Perspective);
  camera3d::State state;
  const camera3d::SceneFrame scene = testScene();
  camera3d::setDefaultCoronalPose(camera, state, scene);

  const glm::vec3 target = state.m_orbitTarget;
  camera3d::panOnSceneAabbPlane(camera, state, scene, glm::vec2{0.0f}, glm::vec2{0.0f}, glm::vec2{0.1f, -0.2f});

  REQUIRE(state.m_panPlanePoint.has_value());
  REQUIRE(state.m_panPlaneNormal.has_value());
  CHECK(glm::distance(state.m_orbitTarget, target) > 0.0f);
}

TEST_CASE("3D scene-AABB pan clamps too-near inside-scene pick planes", "[camera][3d]")
{
  Camera camera(ProjectionType::Perspective);
  camera3d::State state;
  const camera3d::SceneFrame scene{
    .m_center = glm::vec3{0.0f},
    .m_size = glm::vec3{30.0f, 2.0f, 30.0f},
    .m_voxelDiagonal = 1.0f};
  camera3d::setDefaultCoronalPose(camera, state, scene);

  const float distanceToSceneCenter = glm::distance(helper::worldOrigin(camera), state.m_orbitTarget);
  helper::translateAboutCamera(camera, Directions::View::Front, distanceToSceneCenter);

  camera3d::panOnSceneAabbPlane(camera, state, scene, glm::vec2{0.0f}, glm::vec2{0.0f}, glm::vec2{0.05f, 0.0f});

  REQUIRE(state.m_panPlanePoint.has_value());
  const glm::vec3 eyeToPlane = *state.m_panPlanePoint - helper::worldOrigin(camera);
  const float frontDepth = glm::dot(eyeToPlane, helper::worldDirection(camera, Directions::View::Front));
  CHECK(frontDepth == Catch::Approx(state.m_minPanDistance).margin(1.0e-4f));
}

TEST_CASE("3D pan sensitivity does not collapse near the orbit target", "[camera][3d]")
{
  const camera3d::SceneFrame scene = testScene();
  const glm::vec2 oldPos{0.0f};
  const glm::vec2 newPos{0.1f, -0.2f};

  Camera defaultCamera(ProjectionType::Perspective);
  camera3d::State defaultState;
  camera3d::setDefaultCoronalPose(defaultCamera, defaultState, scene);
  const glm::vec3 defaultTarget = defaultState.m_orbitTarget;
  camera3d::pan(defaultCamera, defaultState, oldPos, newPos);
  const float defaultPanDistance = glm::distance(defaultState.m_orbitTarget, defaultTarget);

  Camera closeCamera(ProjectionType::Perspective);
  camera3d::State closeState;
  camera3d::setDefaultCoronalPose(closeCamera, closeState, scene);
  const float distanceToTarget = glm::distance(helper::worldOrigin(closeCamera), closeState.m_orbitTarget);
  helper::translateAboutCamera(closeCamera, Directions::View::Front, distanceToTarget - 1.0e-3f);

  const glm::vec3 closeTarget = closeState.m_orbitTarget;
  camera3d::pan(closeCamera, closeState, oldPos, newPos);
  const float closePanDistance = glm::distance(closeState.m_orbitTarget, closeTarget);

  CHECK(closePanDistance > 0.2f * defaultPanDistance);
}

TEST_CASE("3D orthographic pan keeps the dragged world point under the pointer", "[camera][3d]")
{
  Camera camera(ProjectionType::Perspective);
  camera3d::State state;
  const camera3d::SceneFrame scene = testScene();
  camera3d::setDefaultCoronalPose(camera, state, scene);
  camera3d::setProjection(camera, state, ProjectionType::Orthographic);

  const glm::vec2 oldPos{-0.35f, 0.2f};
  const glm::vec2 newPos{0.15f, -0.25f};
  const glm::vec3 worldUnderOldPointer = helper::world_T_ndc(camera, glm::vec3{oldPos, 0.0f});

  camera3d::panOnSceneAabbPlane(camera, state, scene, oldPos, oldPos, newPos);

  checkVec3Approx(helper::world_T_ndc(camera, glm::vec3{newPos, 0.0f}), worldUnderOldPointer);
}

TEST_CASE("3D scroll dollies perspective cameras and zooms orthographic cameras", "[camera][3d]")
{
  Camera camera(ProjectionType::Perspective);
  camera3d::State state;
  camera3d::setDefaultCoronalPose(camera, state, testScene());

  const glm::vec3 perspectiveEye = helper::worldOrigin(camera);
  camera3d::dollyOrZoom(camera, state, glm::vec2{0.0f}, 1.0f, false);
  CHECK(glm::distance(helper::worldOrigin(camera), perspectiveEye) > 0.0f);

  camera3d::setProjection(camera, state, ProjectionType::Orthographic);
  const glm::vec3 orthographicEye = helper::worldOrigin(camera);
  const float zoom = camera.getZoom();
  camera3d::dollyOrZoom(camera, state, glm::vec2{0.0f}, 1.0f, false);
  checkVec3Approx(helper::worldOrigin(camera), orthographicEye);
  CHECK(camera.getZoom() > zoom);
}

TEST_CASE("3D perspective and orthographic projections keep independent zoom state", "[camera][3d]")
{
  Camera camera(ProjectionType::Perspective);
  camera3d::State state;
  camera3d::setDefaultCoronalPose(camera, state, testScene());

  camera3d::dollyOrZoom(camera, state, glm::vec2{0.0f}, 1.0f, false, true);
  const float perspectiveZoom = camera.getZoom();
  REQUIRE(perspectiveZoom != Catch::Approx(1.0f));

  camera3d::setProjection(camera, state, ProjectionType::Orthographic);
  CHECK(camera.getZoom() == Catch::Approx(1.0f));

  camera3d::dollyOrZoom(camera, state, glm::vec2{0.0f}, 1.0f, false);
  const float orthographicZoom = camera.getZoom();
  REQUIRE(orthographicZoom != Catch::Approx(1.0f));

  camera3d::setProjection(camera, state, ProjectionType::Perspective);
  CHECK(camera.getZoom() == Catch::Approx(perspectiveZoom));

  camera3d::setProjection(camera, state, ProjectionType::Orthographic);
  CHECK(camera.getZoom() == Catch::Approx(orthographicZoom));
}

TEST_CASE("3D alternate scroll adjusts perspective field of view", "[camera][3d]")
{
  Camera camera(ProjectionType::Perspective);
  camera3d::State state;
  camera3d::setDefaultCoronalPose(camera, state, testScene());

  const glm::vec3 eye = helper::worldOrigin(camera);
  const float angle = camera.angle();

  camera3d::dollyOrZoom(camera, state, glm::vec2{0.0f}, 1.0f, false, true);

  checkVec3Approx(helper::worldOrigin(camera), eye);
  CHECK(camera.angle() < angle);
}

TEST_CASE("3D perspective scroll moves along the pointer ray", "[camera][3d]")
{
  Camera camera(ProjectionType::Perspective);
  camera3d::State state;
  camera3d::setDefaultCoronalPose(camera, state, testScene());

  const glm::vec2 ndcPos{0.25f, -0.1f};
  const glm::vec3 expectedDirection = helper::worldRayDirection(camera, ndcPos);
  const glm::vec3 before = helper::worldOrigin(camera);

  camera3d::dollyOrZoom(camera, state, ndcPos, 1.0f, false);

  const glm::vec3 movement = glm::normalize(helper::worldOrigin(camera) - before);
  CHECK(glm::dot(movement, expectedDirection) == Catch::Approx(1.0f).margin(1.0e-4f));
}

TEST_CASE("3D crosshairs follow is independent of drag rotation style", "[camera][3d]")
{
  Camera camera(ProjectionType::Perspective);
  camera3d::State state;
  camera3d::setDefaultCoronalPose(camera, state, testScene());

  const glm::vec3 initialEye = helper::worldOrigin(camera);
  state.m_viewPositionFollowsCrosshairs = false;
  camera3d::followCrosshairs(camera, state, glm::vec3{10.0f, 20.0f, 30.0f});
  checkVec3Approx(helper::worldOrigin(camera), initialEye);

  state.m_viewPositionFollowsCrosshairs = true;
  state.m_crosshairsFollowOffset = glm::vec3{1.0f, 2.0f, 3.0f};
  camera3d::followCrosshairs(camera, state, glm::vec3{10.0f, 20.0f, 30.0f});
  checkVec3Approx(helper::worldOrigin(camera), glm::vec3{11.0f, 22.0f, 33.0f});

  state.m_crosshairsFollowOffset = glm::vec3{0.0f};
  camera3d::followCrosshairs(camera, state, glm::vec3{-1.0f, -2.0f, -3.0f});
  checkVec3Approx(helper::worldOrigin(camera), glm::vec3{-1.0f, -2.0f, -3.0f});
}
