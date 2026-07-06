#include "logic/camera/Camera3DControls.h"

#include "common/DirectionMaps.h"
#include "common/Geometry.h"
#include "common/MathFuncs.h"
#include "logic/camera/Camera.h"
#include "logic/camera/CameraHelpers.h"
#include "logic/camera/MathUtility.h"

#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <cmath>

namespace
{

constexpr float k_minSceneSize = 1.0e-4f;
constexpr float k_minCameraDistance = k_minSceneSize;
constexpr float k_minScrollDistance = k_minSceneSize;
constexpr float k_defaultOrbitDistanceScale = 0.75f;
constexpr float k_minPanDistanceScale = 0.25f;
constexpr float k_nearVoxelDiagonalFraction = 0.5f;
constexpr float k_clipPlaneVoxelMarginFraction = 1.0f;
constexpr float k_perspectiveScrollScale = 0.01f;
constexpr float k_orthographicScrollScale = 0.22f;
constexpr float k_perspectiveFovScrollScale = 1.03f;
constexpr float k_fastMultiplier = 4.0f;

float finiteAbs(float value)
{
  return std::isfinite(value) ? std::abs(value) : 0.0f;
}

glm::vec3 safeSceneSize(const glm::vec3& size)
{
  return glm::vec3{finiteAbs(size.x), finiteAbs(size.y), finiteAbs(size.z)};
}

glm::vec3 worldCameraRight(const Camera& camera)
{
  return helper::worldDirection(camera, Directions::View::Right);
}

glm::vec3 worldCameraUp(const Camera& camera)
{
  return helper::worldDirection(camera, Directions::View::Up);
}

glm::vec3 worldCameraBack(const Camera& camera)
{
  return helper::worldDirection(camera, Directions::View::Back);
}

glm::vec3 worldCameraFront(const Camera& camera)
{
  return helper::worldDirection(camera, Directions::View::Front);
}

glm::vec3 worldRayOrigin(const Camera& camera, const glm::vec2& ndcPos)
{
  return camera.isOrthographic() ? helper::world_T_ndc(camera, glm::vec3{ndcPos, -1.0f}) : helper::worldOrigin(camera);
}

void setCameraPose(
  Camera& camera,
  const glm::vec3& eye,
  const glm::vec3& right,
  const glm::vec3& up,
  const glm::vec3& back)
{
  const glm::mat4 world_T_camera{
    glm::vec4{glm::normalize(right), 0.0f},
    glm::vec4{glm::normalize(up), 0.0f},
    glm::vec4{glm::normalize(back), 0.0f},
    glm::vec4{eye, 1.0f}};
  camera.set_camera_T_anatomy(glm::inverse(world_T_camera));
}

void setCameraTargetPreservingOrientation(Camera& camera, const glm::vec3& target, float distance)
{
  const glm::vec3 eye = target - distance * worldCameraFront(camera);
  setCameraPose(camera, eye, worldCameraRight(camera), worldCameraUp(camera), worldCameraBack(camera));
}

float distanceToTarget(const Camera& camera, const glm::vec3& target)
{
  return std::max(glm::distance(helper::worldOrigin(camera), target), k_minCameraDistance);
}

AABB<float> sceneAabb(const camera3d::SceneFrame& scene)
{
  const glm::vec3 halfSize = 0.5f * safeSceneSize(scene.m_size);
  return {scene.m_center - halfSize, scene.m_center + halfSize};
}

std::optional<glm::vec3>
raySceneAabbHit(const Camera& camera, const camera3d::SceneFrame& scene, const glm::vec2& ndcPos)
{
  const glm::vec3 rayOrigin = worldRayOrigin(camera, ndcPos);
  const glm::vec3 rayDir = helper::worldRayDirection(camera, ndcPos);
  const auto [boxMin, boxMax] = sceneAabb(scene);
  const auto [hit, entry, exit] = math::slabs(rayOrigin, rayDir, boxMin, boxMax);
  if (!hit || exit <= 0.0f) {
    return std::nullopt;
  }

  const float t = (entry > 0.0f) ? entry : exit;
  if (t <= 0.0f || !std::isfinite(t)) {
    return std::nullopt;
  }

  return rayOrigin + t * rayDir;
}

std::optional<glm::vec3>
rayPointAtMinimumFrontDepth(const Camera& camera, const glm::vec2& ndcPos, float minimumFrontDepth)
{
  constexpr float k_parallelTolerance = 1.0e-6f;
  const glm::vec3 rayOrigin = worldRayOrigin(camera, ndcPos);
  const glm::vec3 rayDir = helper::worldRayDirection(camera, ndcPos);
  const float frontComponent = glm::dot(rayDir, worldCameraFront(camera));
  if (frontComponent < k_parallelTolerance) {
    return std::nullopt;
  }

  return rayOrigin + (minimumFrontDepth / frontComponent) * rayDir;
}

std::optional<glm::vec3> pickPanPlanePoint(
  const Camera& camera,
  const camera3d::SceneFrame& scene,
  const glm::vec2& ndcPos,
  float minimumFrontDepth)
{
  const std::optional<glm::vec3> sceneHit = raySceneAabbHit(camera, scene, ndcPos);
  if (!sceneHit) {
    return std::nullopt;
  }

  const glm::vec3 eyeToHit = *sceneHit - helper::worldOrigin(camera);
  const float hitFrontDepth = glm::dot(eyeToHit, worldCameraFront(camera));
  if (hitFrontDepth >= minimumFrontDepth) {
    return sceneHit;
  }

  return rayPointAtMinimumFrontDepth(camera, ndcPos, minimumFrontDepth);
}

std::optional<glm::vec3>
rayPlaneHit(const Camera& camera, const glm::vec2& ndcPos, const glm::vec3& planePoint, const glm::vec3& planeNormal)
{
  constexpr float k_parallelTolerance = 1.0e-6f;
  const glm::vec3 rayOrigin = worldRayOrigin(camera, ndcPos);
  const glm::vec3 rayDir = helper::worldRayDirection(camera, ndcPos);
  const float denom = glm::dot(rayDir, planeNormal);
  if (std::abs(denom) < k_parallelTolerance) {
    return std::nullopt;
  }

  const float t = glm::dot(planePoint - rayOrigin, planeNormal) / denom;
  if (t <= 0.0f || !std::isfinite(t)) {
    return std::nullopt;
  }

  return rayOrigin + t * rayDir;
}

bool sameDragStart(const std::optional<glm::vec2>& previous, const glm::vec2& current)
{
  return previous && glm::length(*previous - current) < 1.0e-6f;
}

float zoomForProjection(const camera3d::State& state, ProjectionType projectionType)
{
  return ProjectionType::Orthographic == projectionType ? state.m_orthographicZoom : state.m_perspectiveZoom;
}

void saveCurrentProjectionZoom(const Camera& camera, camera3d::State& state)
{
  if (ProjectionType::Orthographic == state.m_projectionType) {
    state.m_orthographicZoom = camera.getZoom();
  }
  else {
    state.m_perspectiveZoom = camera.getZoom();
  }
}

void applyPanTranslation(Camera& camera, camera3d::State& state, const glm::vec3& worldTranslation)
{
  helper::translateAboutCamera(camera, glm::vec3{camera.camera_T_world() * glm::vec4{worldTranslation, 0.0f}});
  state.m_orbitTarget += worldTranslation;
  if (state.m_viewPositionFollowsCrosshairs) {
    state.m_crosshairsFollowOffset += worldTranslation;
  }
  state.m_orbitDistance = distanceToTarget(camera, state.m_orbitTarget);
  camera3d::markUserMoved(state);
}

void panOrthographic(Camera& camera, camera3d::State& state, const glm::vec2& ndcOldPos, const glm::vec2& ndcNewPos)
{
  // In orthographic projection, world-space cursor deltas are independent of depth. Translating by
  // old - new makes the same world point stay underneath the mouse pointer during a drag.
  const glm::vec3 oldWorld = helper::world_T_ndc(camera, glm::vec3{ndcOldPos, 0.0f});
  const glm::vec3 newWorld = helper::world_T_ndc(camera, glm::vec3{ndcNewPos, 0.0f});
  applyPanTranslation(camera, state, oldWorld - newWorld);
}

} // namespace

namespace camera3d
{

SceneFrame sceneFrameFromAABB(const AABB<float>& worldBox)
{
  return SceneFrame{
    .m_center = math::computeAABBoxCenter(worldBox),
    .m_size = safeSceneSize(math::computeAABBoxSize(worldBox)),
    .m_voxelDiagonal = 1.0f};
}

float sceneDiagonal(const glm::vec3& sceneSize)
{
  return std::max(glm::length(safeSceneSize(sceneSize)), k_minSceneSize);
}

float sceneVoxelDiagonal(float voxelDiagonal)
{
  return std::max(finiteAbs(voxelDiagonal), k_minSceneSize);
}

float defaultOrbitDistance(const glm::vec3& sceneSize)
{
  return std::max(k_defaultOrbitDistanceScale * sceneDiagonal(sceneSize), k_minCameraDistance);
}

SceneMetrics sceneMetrics(const SceneFrame& scene)
{
  const glm::vec3 size = safeSceneSize(scene.m_size);
  const float diagonal = sceneDiagonal(size);
  const float voxelDiagonal = sceneVoxelDiagonal(scene.m_voxelDiagonal);
  return SceneMetrics{
    .m_diagonal = diagonal,
    .m_voxelDiagonal = voxelDiagonal,
    .m_defaultOrbitDistance = std::max(k_defaultOrbitDistanceScale * diagonal, k_minCameraDistance),
    .m_minTargetDistance = std::max(0.55f * diagonal, k_minCameraDistance),
    .m_minPanDistance = std::max(k_minPanDistanceScale * diagonal, k_minCameraDistance),
    .m_scrollDistance = std::max(k_perspectiveScrollScale * diagonal, k_minScrollDistance),
    .m_defaultFov = glm::vec2{std::max(glm::compMax(size), k_minSceneSize)}};
}

Pose defaultCoronalPose(const SceneFrame& scene)
{
  const SceneMetrics metrics = sceneMetrics(scene);
  return Pose{
    .m_eye = scene.m_center + metrics.m_defaultOrbitDistance * Directions::get(Directions::Cartesian::NegY),
    .m_right = Directions::get(Directions::Cartesian::PosX),
    .m_up = Directions::get(Directions::Cartesian::PosZ),
    .m_back = Directions::get(Directions::Cartesian::NegY)};
}

void configureClipPlanes(Camera& camera, const SceneFrame& scene, float eyeToTargetDistance)
{
  const SceneMetrics metrics = sceneMetrics(scene);
  const float diagonal = metrics.m_diagonal;
  const float radius = 0.5f * diagonal;
  const float voxelMargin = k_clipPlaneVoxelMarginFraction * metrics.m_voxelDiagonal;
  const float minNearDistance = std::max(k_nearVoxelDiagonalFraction * metrics.m_voxelDiagonal, k_minSceneSize);
  const float distanceToNearScene = eyeToTargetDistance - radius;
  const float nearDistance = std::max(minNearDistance, distanceToNearScene - voxelMargin);
  const float farDistance = std::max(eyeToTargetDistance + radius + voxelMargin, nearDistance + diagonal);
  camera.setNearDistance(nearDistance);
  camera.setFarDistance(farDistance);
}

void setDefaultCoronalPose(Camera& camera, State& state, const SceneFrame& scene)
{
  Controller{camera, state}.initializeDefaultPose(scene);
}

void recenter(Camera& camera, State& state, const SceneFrame& scene, const glm::vec3& target)
{
  Controller{camera, state}.recenter(scene, target);
}

void setProjection(Camera& camera, State& state, ProjectionType projectionType)
{
  Controller{camera, state}.setProjection(projectionType);
}

void orbit(Camera& camera, State& state, const glm::vec2& ndcOldPos, const glm::vec2& ndcNewPos)
{
  Controller{camera, state}.orbit(ndcOldPos, ndcNewPos);
}

void rotateAboutEye(Camera& camera, State& state, const glm::vec2& ndcOldPos, const glm::vec2& ndcNewPos)
{
  Controller{camera, state}.rotateAboutEye(ndcOldPos, ndcNewPos);
}

void roll(Camera& camera, State& state, const glm::vec2& ndcOldPos, const glm::vec2& ndcNewPos)
{
  Controller{camera, state}.roll(ndcOldPos, ndcNewPos);
}

void pan(Camera& camera, State& state, const glm::vec2& ndcOldPos, const glm::vec2& ndcNewPos)
{
  Controller{camera, state}.pan(ndcOldPos, ndcNewPos);
}

void panOnSceneAabbPlane(
  Camera& camera,
  State& state,
  const SceneFrame& scene,
  const glm::vec2& ndcStartPos,
  const glm::vec2& ndcOldPos,
  const glm::vec2& ndcNewPos)
{
  if (camera.isOrthographic()) {
    panOrthographic(camera, state, ndcOldPos, ndcNewPos);
    state.m_panDragStartNdc = std::nullopt;
    state.m_panPlanePoint = std::nullopt;
    state.m_panPlaneNormal = std::nullopt;
    return;
  }

  if (!sameDragStart(state.m_panDragStartNdc, ndcStartPos)) {
    state.m_panDragStartNdc = ndcStartPos;
    state.m_panPlanePoint = pickPanPlanePoint(camera, scene, ndcStartPos, state.m_minPanDistance);
    state.m_panPlaneNormal = worldCameraFront(camera);
  }

  if (!state.m_panPlanePoint || !state.m_panPlaneNormal) {
    pan(camera, state, ndcOldPos, ndcNewPos);
    return;
  }

  const std::optional<glm::vec3> oldHit =
    rayPlaneHit(camera, ndcOldPos, *state.m_panPlanePoint, *state.m_panPlaneNormal);
  const std::optional<glm::vec3> newHit =
    rayPlaneHit(camera, ndcNewPos, *state.m_panPlanePoint, *state.m_panPlaneNormal);
  if (!oldHit || !newHit) {
    pan(camera, state, ndcOldPos, ndcNewPos);
    return;
  }

  const glm::vec3 translation = *oldHit - *newHit;
  applyPanTranslation(camera, state, translation);
}

void dollyOrZoom(
  Camera& camera,
  State& state,
  const glm::vec2& ndcPos,
  float scrollDelta,
  bool faster,
  bool adjustPerspectiveFov)
{
  Controller{camera, state}.scroll(ndcPos, scrollDelta, faster, adjustPerspectiveFov);
}

void followCrosshairs(Camera& camera, State& state, const glm::vec3& crosshairs)
{
  Controller{camera, state}.followCrosshairs(crosshairs);
}

void markUserMoved(State& state)
{
  state.m_userMovedCamera = true;
}

Controller::Controller(Camera& camera, State& state) : m_camera(camera), m_state(state) {}

void Controller::initializeDefaultPose(const SceneFrame& scene)
{
  const SceneMetrics metrics = sceneMetrics(scene);
  const Pose pose = defaultCoronalPose(scene);
  const float perspectiveZoom = m_state.m_perspectiveZoom;
  const float orthographicZoom = m_state.m_orthographicZoom;

  m_state.m_orbitTarget = scene.m_center;
  m_state.m_orbitDistance = metrics.m_defaultOrbitDistance;
  m_state.m_minTargetDistance = metrics.m_minTargetDistance;
  m_state.m_minPanDistance = metrics.m_minPanDistance;
  m_state.m_scrollDistance = metrics.m_scrollDistance;
  m_state.m_panDragStartNdc = std::nullopt;
  m_state.m_panPlanePoint = std::nullopt;
  m_state.m_panPlaneNormal = std::nullopt;
  m_state.m_perspectiveZoom = perspectiveZoom;
  m_state.m_orthographicZoom = orthographicZoom;

  m_camera.setDefaultFov(metrics.m_defaultFov);
  m_camera.setZoom(zoomForProjection(m_state, m_state.m_projectionType));
  configureClipPlanes(m_camera, scene, m_state.m_orbitDistance);
  setCameraPose(m_camera, pose.m_eye, pose.m_right, pose.m_up, pose.m_back);
}

void Controller::updateScene(const SceneFrame& scene)
{
  const SceneMetrics metrics = sceneMetrics(scene);
  m_state.m_minTargetDistance = metrics.m_minTargetDistance;
  m_state.m_minPanDistance = metrics.m_minPanDistance;
  m_state.m_scrollDistance = metrics.m_scrollDistance;
  configureClipPlanes(m_camera, scene, distanceToTarget(m_camera, m_state.m_orbitTarget));
}

void Controller::recenter(const SceneFrame& scene, const glm::vec3& target)
{
  const SceneMetrics metrics = sceneMetrics(scene);
  const float perspectiveZoom = m_state.m_perspectiveZoom;
  const float orthographicZoom = m_state.m_orthographicZoom;
  m_state.m_orbitTarget = target;
  m_state.m_crosshairsFollowOffset = glm::vec3{0.0f};
  m_state.m_orbitDistance = metrics.m_defaultOrbitDistance;
  m_state.m_minTargetDistance = metrics.m_minTargetDistance;
  m_state.m_minPanDistance = metrics.m_minPanDistance;
  m_state.m_scrollDistance = metrics.m_scrollDistance;
  m_state.m_perspectiveZoom = perspectiveZoom;
  m_state.m_orthographicZoom = orthographicZoom;
  m_camera.setDefaultFov(metrics.m_defaultFov);
  m_camera.setZoom(zoomForProjection(m_state, m_state.m_projectionType));
  configureClipPlanes(m_camera, scene, m_state.m_orbitDistance);
  setCameraTargetPreservingOrientation(m_camera, target, m_state.m_orbitDistance);
  m_state.m_userMovedCamera = false;
}

void Controller::setProjection(ProjectionType projectionType)
{
  if (projectionType == m_state.m_projectionType) {
    return;
  }

  saveCurrentProjectionZoom(m_camera, m_state);
  auto projection = helper::createCameraProjection(projectionType);
  projection->setAspectRatio(m_camera.aspectRatio());
  projection->setDefaultFov(m_camera.projection()->defaultFov());
  projection->setNearDistance(m_camera.nearDistance());
  projection->setFarDistance(m_camera.farDistance());
  projection->setZoom(zoomForProjection(m_state, projectionType));
  m_camera.setProjection(std::move(projection));
  m_state.m_projectionType = projectionType;
  if (ProjectionType::Orthographic == projectionType) {
    m_state.m_viewPositionFollowsCrosshairs = false;
  }
  markUserMoved(m_state);
}

void Controller::orbit(const glm::vec2& ndcOldPos, const glm::vec2& ndcNewPos)
{
  helper::rotateAboutWorldPoint(m_camera, ndcOldPos, ndcNewPos, m_state.m_orbitTarget);
  m_state.m_orbitDistance = distanceToTarget(m_camera, m_state.m_orbitTarget);
  markUserMoved(m_state);
}

void Controller::rotateAboutEye(const glm::vec2& ndcOldPos, const glm::vec2& ndcNewPos)
{
  helper::rotateAboutCameraOrigin(m_camera, ndcOldPos, ndcNewPos);
  m_state.m_orbitDistance = distanceToTarget(m_camera, m_state.m_orbitTarget);
  markUserMoved(m_state);
}

void Controller::roll(const glm::vec2& ndcOldPos, const glm::vec2& ndcNewPos)
{
  helper::rotateInPlane(m_camera, ndcOldPos, ndcNewPos, glm::vec2{0.0f});
  markUserMoved(m_state);
}

void Controller::pan(const glm::vec2& ndcOldPos, const glm::vec2& ndcNewPos)
{
  const float panDistance = std::max(distanceToTarget(m_camera, m_state.m_orbitTarget), m_state.m_minPanDistance);
  const glm::vec3 panPlaneTarget = helper::worldOrigin(m_camera) + panDistance * worldCameraFront(m_camera);
  const float ndcZ = helper::ndcZofWorldPoint(m_camera, panPlaneTarget);
  const glm::vec3 translation = -helper::translationInCameraPlane(m_camera, ndcOldPos, ndcNewPos, ndcZ);
  applyPanTranslation(m_camera, m_state, translation);
}

void Controller::scroll(const glm::vec2& ndcPos, float scrollDelta, bool faster, bool adjustPerspectiveFov)
{
  const float multiplier = faster ? k_fastMultiplier : 1.0f;

  if (ProjectionType::Orthographic == m_state.m_projectionType) {
    helper::zoomNdcDelta(m_camera, multiplier * k_orthographicScrollScale * scrollDelta, ndcPos);
    m_state.m_orthographicZoom = m_camera.getZoom();
  }
  else if (adjustPerspectiveFov) {
    m_camera.setZoom(m_camera.getZoom() * std::pow(k_perspectiveFovScrollScale, multiplier * scrollDelta));
    m_state.m_perspectiveZoom = m_camera.getZoom();
  }
  else {
    const glm::vec3 cameraVec =
      multiplier * m_state.m_scrollDistance * scrollDelta * helper::cameraRayDirection(m_camera, ndcPos);
    helper::translateAboutCamera(m_camera, cameraVec);
    if (m_state.m_viewPositionFollowsCrosshairs) {
      m_state.m_crosshairsFollowOffset += glm::vec3{m_camera.world_T_camera() * glm::vec4{cameraVec, 0.0f}};
    }
    m_state.m_orbitDistance = distanceToTarget(m_camera, m_state.m_orbitTarget);
  }
  markUserMoved(m_state);
}

void Controller::followCrosshairs(const glm::vec3& crosshairs)
{
  if (!m_state.m_viewPositionFollowsCrosshairs) {
    return;
  }

  setCameraPose(
    m_camera,
    crosshairs + m_state.m_crosshairsFollowOffset,
    worldCameraRight(m_camera),
    worldCameraUp(m_camera),
    worldCameraBack(m_camera));
}

} // namespace camera3d
