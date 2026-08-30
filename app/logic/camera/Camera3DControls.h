#pragma once

#include "common/AABB.h"
#include "logic/camera/CameraTypes.h"

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include <optional>

class Camera;

namespace camera3d
{

/**
 * @brief Target selection used by 3D orbit interactions
 */
enum class OrbitTargetMode
{
  /** @brief Use the center of the image bounds visible in the 3D view */
  VisibleImages,
  /** @brief Use the application's current crosshairs position */
  Crosshairs
};

/**
 * @brief Mutable state owned by a 3D view camera
 */
struct State
{
  /** @brief Source used to choose the orbit target */
  OrbitTargetMode m_orbitTargetMode = OrbitTargetMode::VisibleImages;
  /** @brief Active camera projection type */
  ProjectionType m_projectionType = ProjectionType::Perspective;
  /** @brief Current orbit target in World space */
  glm::vec3 m_orbitTarget{0.0f};
  /** @brief Offset preserved while camera position follows crosshairs */
  glm::vec3 m_crosshairsFollowOffset{0.0f};
  /** @brief NDC position at which the current scene-plane pan drag started */
  std::optional<glm::vec2> m_panDragStartNdc;
  /** @brief World-space point anchoring the current scene-plane pan */
  std::optional<glm::vec3> m_panPlanePoint;
  /** @brief World-space normal of the current scene-plane pan plane */
  std::optional<glm::vec3> m_panPlaneNormal;
  /** @brief Current eye-to-orbit-target distance */
  float m_orbitDistance = 1.0f;
  /** @brief Minimum target distance used to keep the camera outside tiny scenes */
  float m_minTargetDistance = 1.0f;
  /** @brief Minimum panning depth used for stable perspective panning */
  float m_minPanDistance = 1.0f;
  /** @brief World distance moved by one perspective scroll unit before modifiers */
  float m_scrollDistance = 1.0f;
  /** @brief Stored zoom/FOV scale for perspective projection */
  float m_perspectiveZoom = 1.0f;
  /** @brief Stored zoom scale for orthographic projection */
  float m_orthographicZoom = 1.0f;
  /** @brief True when the camera eye follows the global crosshairs position */
  bool m_viewPositionFollowsCrosshairs = false;
  /** @brief True when orthogonal image planes are drawn in the 3D view */
  bool m_showImagePlanes = true;
  /** @brief True after the user has manually changed the 3D camera */
  bool m_userMovedCamera = false;
};

/**
 * @brief World-space scene bounds and sampling scale used to frame a 3D view
 */
struct SceneFrame
{
  /** @brief Center of the visible scene in World space */
  glm::vec3 m_center{0.0f};
  /** @brief Size of the visible scene along World axes */
  glm::vec3 m_size{2.0f};
  /** @brief Smallest relevant voxel diagonal in World millimeters */
  float m_voxelDiagonal = 1.0f;
};

/**
 * @brief Derived scene measurements used by 3D camera controls
 */
struct SceneMetrics
{
  /** @brief Length of the scene bounding-box diagonal */
  float m_diagonal = 1.0f;
  /** @brief Clamped voxel diagonal used for clip-plane spacing */
  float m_voxelDiagonal = 1.0f;
  /** @brief Default distance from the camera eye to the orbit target */
  float m_defaultOrbitDistance = 1.0f;
  /** @brief Minimum distance used when framing a target */
  float m_minTargetDistance = 1.0f;
  /** @brief Minimum depth used by perspective panning */
  float m_minPanDistance = 1.0f;
  /** @brief World distance moved by one perspective scroll unit */
  float m_scrollDistance = 1.0f;
  /** @brief Default projection field of view extent */
  glm::vec2 m_defaultFov{1.0f};
};

/**
 * @brief World-space 3D camera pose basis
 */
struct Pose
{
  /** @brief Camera eye position in World space */
  glm::vec3 m_eye{0.0f};
  /** @brief Camera right direction in World space */
  glm::vec3 m_right{1.0f, 0.0f, 0.0f};
  /** @brief Camera up direction in World space */
  glm::vec3 m_up{0.0f, 0.0f, 1.0f};
  /** @brief Camera back direction in World space */
  glm::vec3 m_back{0.0f, -1.0f, 0.0f};
};

/**
 * @brief Build 3D camera framing data from world-space visible image bounds
 * @param worldBox Axis-aligned bounds of visible 3D content in World space
 * @return Scene frame centered on `worldBox`
 */
SceneFrame sceneFrameFromAABB(const AABB<float>& worldBox);

/**
 * @brief Compute a clamped scene diagonal length
 * @param sceneSize Scene size along World axes
 * @return Positive diagonal length
 */
float sceneDiagonal(const glm::vec3& sceneSize);

/**
 * @brief Clamp a voxel diagonal to a useful positive value
 * @param voxelDiagonal Candidate voxel diagonal
 * @return Positive voxel diagonal
 */
float sceneVoxelDiagonal(float voxelDiagonal);

/**
 * @brief Pick the default orbit distance for the visible scene
 * @param sceneSize Scene size along World axes
 * @return Conservative camera distance that starts outside visible anatomy
 */
float defaultOrbitDistance(const glm::vec3& sceneSize);

/**
 * @brief Compute derived metrics used by 3D camera controls
 * @param scene Scene frame to measure
 * @return Derived scene metrics
 */
SceneMetrics sceneMetrics(const SceneFrame& scene);

/**
 * @brief Compute Entropy's default 3D coronal camera pose without mutating camera state
 * @param scene Scene frame to view
 * @return Default pose with camera forward toward +Y and up toward +Z
 */
Pose defaultCoronalPose(const SceneFrame& scene);

/**
 * @brief Set near and far clipping planes from the scene extent and current camera pose
 * @param camera Camera whose projection clip distances are updated
 * @param scene Scene frame to keep visible
 * @param eyeToTargetDistance Reserved eye-to-target distance parameter
 */
void configureClipPlanes(Camera& camera, const SceneFrame& scene, float);

/**
 * @brief Reset to Entropy's default 3D pose
 * @param camera Camera to mutate
 * @param state 3D camera state to update
 * @param scene Scene frame used for default placement
 * @throw Propagates exceptions from camera projection access
 */
void setDefaultCoronalPose(Camera& camera, State& state, const SceneFrame& scene);

/**
 * @brief Reset the 3D camera pose and projection scale while preserving view options
 * @param camera Camera to mutate
 * @param state 3D camera state to reset
 * @param scene Scene frame used for default placement
 * @param target World-space target to place at the center of the reset view
 * @throw Propagates exceptions from projection allocation
 */
void resetView(Camera& camera, State& state, const SceneFrame& scene, const glm::vec3& target);

/**
 * @brief Recenter a crosshairs-following camera without letting orbit state move the eye
 * @param camera Camera to mutate
 * @param state 3D camera state to update
 * @param scene Scene frame used for clip planes and distances
 * @param crosshairs Crosshairs position used as the camera eye
 * @param orbitTarget World-space target retained for orbit calculations
 * @throw Propagates exceptions from camera projection access
 */
void recenterFollowing(
  Camera& camera,
  State& state,
  const SceneFrame& scene,
  const glm::vec3& crosshairs,
  const glm::vec3& orbitTarget);

/**
 * @brief Reset a crosshairs-following camera without letting orbit state move the eye
 * @param camera Camera to mutate
 * @param state 3D camera state to reset
 * @param scene Scene frame used for default placement
 * @param crosshairs Crosshairs position used as the camera eye
 * @param orbitTarget World-space target retained for orbit calculations
 * @throw Propagates exceptions from projection allocation
 */
void resetFollowing(
  Camera& camera,
  State& state,
  const SceneFrame& scene,
  const glm::vec3& crosshairs,
  const glm::vec3& orbitTarget);

/**
 * @brief Reframe the camera around the requested target without changing interaction options
 * @param camera Camera to mutate
 * @param state 3D camera state to update
 * @param scene Scene frame used for distances and clipping
 * @param target World-space target for recentering
 * @throw Propagates exceptions from camera projection access
 */
void recenter(Camera& camera, State& state, const SceneFrame& scene, const glm::vec3& target);

/**
 * @brief Change between perspective and orthographic projection while preserving stored zooms
 * @param camera Camera to mutate
 * @param state 3D camera state to update
 * @param projectionType Projection type to activate
 * @throw Propagates exceptions from projection allocation
 */
void setProjection(Camera& camera, State& state, ProjectionType projectionType);

/**
 * @brief Orbit the camera around the state's current orbit target
 * @param camera Camera to mutate
 * @param state 3D camera state to update
 * @param ndcOldPos Previous pointer position in NDC
 * @param ndcNewPos Current pointer position in NDC
 */
void orbit(Camera& camera, State& state, const glm::vec2& ndcOldPos, const glm::vec2& ndcNewPos);

/**
 * @brief Rotate the camera about its current eye position
 * @param camera Camera to mutate
 * @param state 3D camera state to update
 * @param ndcOldPos Previous pointer position in NDC
 * @param ndcNewPos Current pointer position in NDC
 */
void rotateAboutEye(Camera& camera, State& state, const glm::vec2& ndcOldPos, const glm::vec2& ndcNewPos);

/**
 * @brief Roll the camera in the view plane
 * @param camera Camera to mutate
 * @param state 3D camera state to update
 * @param ndcOldPos Previous pointer position in NDC
 * @param ndcNewPos Current pointer position in NDC
 */
void roll(Camera& camera, State& state, const glm::vec2& ndcOldPos, const glm::vec2& ndcNewPos);

/**
 * @brief Pan the camera parallel to the view plane
 * @param camera Camera to mutate
 * @param state 3D camera state to update
 * @param ndcOldPos Previous pointer position in NDC
 * @param ndcNewPos Current pointer position in NDC
 */
void pan(Camera& camera, State& state, const glm::vec2& ndcOldPos, const glm::vec2& ndcNewPos);

/**
 * @brief Pan the camera by intersecting pointer rays with the visible scene's pan plane
 * @param camera Camera to mutate
 * @param state 3D camera state to update
 * @param scene Visible scene frame used to choose the pan plane
 * @param ndcStartPos Pointer position at drag start in NDC
 * @param ndcOldPos Previous pointer position in NDC
 * @param ndcNewPos Current pointer position in NDC
 */
void panOnSceneAabbPlane(
  Camera& camera,
  State& state,
  const SceneFrame& scene,
  const glm::vec2& ndcStartPos,
  const glm::vec2& ndcOldPos,
  const glm::vec2& ndcNewPos);

/**
 * @brief Apply scroll input as perspective dolly, orthographic zoom, or perspective FOV change
 * @param camera Camera to mutate
 * @param state 3D camera state to update
 * @param ndcPos Scroll position in NDC
 * @param scrollDelta Scroll delta
 * @param faster True to apply the fast-scroll multiplier
 * @param adjustPerspectiveFov True to adjust perspective FOV instead of dollying
 */
void dollyOrZoom(
  Camera& camera,
  State& state,
  const glm::vec2& ndcPos,
  float scrollDelta,
  bool faster,
  bool adjustPerspectiveFov = false);

/**
 * @brief Move a following camera to the crosshairs plus its stored follow offset
 * @param camera Camera to mutate
 * @param state 3D camera state containing follow mode and offset
 * @param crosshairs Current crosshairs position in World space
 */
void followCrosshairs(Camera& camera, State& state, const glm::vec3& crosshairs);

/**
 * @brief Mark the 3D camera state as manually moved by the user
 * @param state 3D camera state to update
 */
void markUserMoved(State& state);

/**
 * @brief Stateful controller for applying 3D camera operations to a Camera and State pair
 */
class Controller
{
public:
  /**
   * @brief Construct a controller over existing camera and state storage
   * @param camera Camera to mutate
   * @param state 3D camera state to mutate
   */
  Controller(Camera& camera, State& state);

  /**
   * @brief Initialize the default coronal pose and scene-derived distances
   * @param scene Scene frame used for placement and clipping
   * @throw Propagates exceptions from camera projection access
   */
  void initializeDefaultPose(const SceneFrame& scene);
  /**
   * @brief Update scene-derived distances and clip planes without changing the camera pose
   * @param scene Scene frame used for distances and clipping
   */
  void updateScene(const SceneFrame& scene);
  /**
   * @brief Recenter the camera on a target while preserving projection mode
   * @param scene Scene frame used for distances and clipping
   * @param target World-space target
   * @throw Propagates exceptions from camera projection access
   */
  void recenter(const SceneFrame& scene, const glm::vec3& target);
  /**
   * @brief Recenter a crosshairs-following camera at the crosshairs
   * @param scene Scene frame used for distances and clipping
   * @param crosshairs Crosshairs position used as the camera eye
   * @param orbitTarget World-space target retained for orbit calculations
   * @throw Propagates exceptions from camera projection access
   */
  void recenterFollowing(const SceneFrame& scene, const glm::vec3& crosshairs, const glm::vec3& orbitTarget);
  /**
   * @brief Change the active projection type
   * @param projectionType Projection type to activate
   * @throw Propagates exceptions from projection allocation
   */
  void setProjection(ProjectionType projectionType);
  /**
   * @brief Orbit the camera around the current orbit target
   * @param ndcOldPos Previous pointer position in NDC
   * @param ndcNewPos Current pointer position in NDC
   */
  void orbit(const glm::vec2& ndcOldPos, const glm::vec2& ndcNewPos);
  /**
   * @brief Rotate the camera about its eye position
   * @param ndcOldPos Previous pointer position in NDC
   * @param ndcNewPos Current pointer position in NDC
   */
  void rotateAboutEye(const glm::vec2& ndcOldPos, const glm::vec2& ndcNewPos);
  /**
   * @brief Roll the camera in the view plane
   * @param ndcOldPos Previous pointer position in NDC
   * @param ndcNewPos Current pointer position in NDC
   */
  void roll(const glm::vec2& ndcOldPos, const glm::vec2& ndcNewPos);
  /**
   * @brief Pan the camera parallel to the view plane
   * @param ndcOldPos Previous pointer position in NDC
   * @param ndcNewPos Current pointer position in NDC
   */
  void pan(const glm::vec2& ndcOldPos, const glm::vec2& ndcNewPos);
  /**
   * @brief Apply scroll input to the 3D camera
   * @param ndcPos Scroll position in NDC
   * @param scrollDelta Scroll delta
   * @param faster True to apply the fast-scroll multiplier
   * @param adjustPerspectiveFov True to adjust perspective FOV instead of dollying
   */
  void scroll(const glm::vec2& ndcPos, float scrollDelta, bool faster, bool adjustPerspectiveFov);
  /**
   * @brief Move a following camera to the crosshairs plus its stored follow offset
   * @param crosshairs Current crosshairs position in World space
   */
  void followCrosshairs(const glm::vec3& crosshairs);

private:
  /** @brief Camera mutated by this controller */
  Camera& m_camera;
  /** @brief 3D interaction state mutated by this controller */
  State& m_state;
};

} // namespace camera3d
