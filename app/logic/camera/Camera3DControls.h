#pragma once

#include "common/AABB.h"
#include "logic/camera/CameraTypes.h"

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include <optional>

class Camera;

namespace camera3d
{

enum class OrbitTargetMode
{
  /// Use the center of the image bounds visible in the 3D view.
  VisibleImages,
  /// Use the application's current crosshairs position.
  Crosshairs
};

struct State
{
  OrbitTargetMode m_orbitTargetMode = OrbitTargetMode::VisibleImages;
  ProjectionType m_projectionType = ProjectionType::Perspective;
  glm::vec3 m_orbitTarget{0.0f};
  glm::vec3 m_crosshairsFollowOffset{0.0f};
  std::optional<glm::vec2> m_panDragStartNdc;
  std::optional<glm::vec3> m_panPlanePoint;
  std::optional<glm::vec3> m_panPlaneNormal;
  float m_orbitDistance = 1.0f;
  float m_minTargetDistance = 1.0f;
  float m_minPanDistance = 1.0f;
  float m_scrollDistance = 1.0f;
  float m_perspectiveZoom = 1.0f;
  float m_orthographicZoom = 1.0f;
  bool m_viewPositionFollowsCrosshairs = false;
  bool m_userMovedCamera = false;
};

struct SceneFrame
{
  glm::vec3 m_center{0.0f};
  glm::vec3 m_size{2.0f};
  float m_voxelDiagonal = 1.0f;
};

struct SceneMetrics
{
  float m_diagonal = 1.0f;
  float m_voxelDiagonal = 1.0f;
  float m_defaultOrbitDistance = 1.0f;
  float m_minTargetDistance = 1.0f;
  float m_minPanDistance = 1.0f;
  float m_scrollDistance = 1.0f;
  glm::vec2 m_defaultFov{1.0f};
};

struct Pose
{
  glm::vec3 m_eye{0.0f};
  glm::vec3 m_right{1.0f, 0.0f, 0.0f};
  glm::vec3 m_up{0.0f, 0.0f, 1.0f};
  glm::vec3 m_back{0.0f, -1.0f, 0.0f};
};

/// Build the 3D camera framing data from the world-space visible image bounds.
SceneFrame sceneFrameFromAABB(const AABB<float>& worldBox);

float sceneDiagonal(const glm::vec3& sceneSize);

float sceneVoxelDiagonal(float voxelDiagonal);

/// Pick a conservative camera distance so the initial eye sits outside the visible anatomy.
float defaultOrbitDistance(const glm::vec3& sceneSize);

SceneMetrics sceneMetrics(const SceneFrame& scene);

/// Compute Entropy's default 3D camera pose without mutating Camera state.
Pose defaultCoronalPose(const SceneFrame& scene);

/// Set near/far clipping planes from the scene extent and current eye-to-target distance.
void configureClipPlanes(Camera& camera, const SceneFrame& scene, float eyeToTargetDistance);

/// Reset to Entropy's default 3D pose: coronal, view front +Y, view up +Z.
void setDefaultCoronalPose(Camera& camera, State& state, const SceneFrame& scene);

/// Reframe the camera around the requested target without changing the selected interaction mode.
void recenter(Camera& camera, State& state, const SceneFrame& scene, const glm::vec3& target);

void setProjection(Camera& camera, State& state, ProjectionType projectionType);

void orbit(Camera& camera, State& state, const glm::vec2& ndcOldPos, const glm::vec2& ndcNewPos);

void rotateAboutEye(Camera& camera, State& state, const glm::vec2& ndcOldPos, const glm::vec2& ndcNewPos);

void roll(Camera& camera, State& state, const glm::vec2& ndcOldPos, const glm::vec2& ndcNewPos);

void pan(Camera& camera, State& state, const glm::vec2& ndcOldPos, const glm::vec2& ndcNewPos);

void panOnSceneAabbPlane(
  Camera& camera,
  State& state,
  const SceneFrame& scene,
  const glm::vec2& ndcStartPos,
  const glm::vec2& ndcOldPos,
  const glm::vec2& ndcNewPos);

void dollyOrZoom(
  Camera& camera,
  State& state,
  const glm::vec2& ndcPos,
  float scrollDelta,
  bool faster,
  bool adjustPerspectiveFov = false);

void followCrosshairs(Camera& camera, State& state, const glm::vec3& crosshairs);

void markUserMoved(State& state);

class Controller
{
public:
  Controller(Camera& camera, State& state);

  void initializeDefaultPose(const SceneFrame& scene);
  void updateScene(const SceneFrame& scene);
  void recenter(const SceneFrame& scene, const glm::vec3& target);
  void setProjection(ProjectionType projectionType);
  void orbit(const glm::vec2& ndcOldPos, const glm::vec2& ndcNewPos);
  void rotateAboutEye(const glm::vec2& ndcOldPos, const glm::vec2& ndcNewPos);
  void roll(const glm::vec2& ndcOldPos, const glm::vec2& ndcNewPos);
  void pan(const glm::vec2& ndcOldPos, const glm::vec2& ndcNewPos);
  void scroll(const glm::vec2& ndcPos, float scrollDelta, bool faster, bool adjustPerspectiveFov);
  void followCrosshairs(const glm::vec3& crosshairs);

private:
  Camera& m_camera;
  State& m_state;
};

} // namespace camera3d
