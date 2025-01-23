#ifndef CAMERA_H
#define CAMERA_H

#include "common/CoordinateFrame.h"
#include "common/PublicTypes.h"

#include "logic/camera/Projection.h"

#include <glm/fwd.hpp>

#include <functional>
#include <memory>
#include <optional>

namespace camera
{

/**
 * @brief Camera mapping World space to OpenGL Clip space via a sequence of transformations:
 * clip_O_world = clip_O_camera * camera_O_world, where camera_O_world is further decomposed as
 * camera_O_world = camera_O_anatomy * anatomy_O_start * start_O_world.
 *
 * Clip: Standard OpenGL clip space
 * Camera: Space of the camera
 * Anatomy: Anatomical frame of reference
 * Start: Starting frame of reference
 * World: World space, common to all objects of the scene
 *
 * 1) camera_O_world is a rigid-body matrix, sometimes referred to as the View transformation that
 * maps World to Camera space. Its parts are
 *
 *    a) camera_O_anatomy: User manipulations applied to the camera AFTER the anatomical transformation
 *    b) anatomy_O_start: Anatomical starting frame of reference that is linked to an external callback
 *    c) start_O_world: User manipulations applied to the camera BEFORE the anatomical transformation
 *
 * 2) clip_O_camera is a perspective projection: either orthogonal or perpspective.
 */
class Camera
{
public:
  /// Construct a camera with a projection (either orthographic or perspective) and
  /// a functional that returns the camera's starting coordinate frame. If no functional is
  /// supplied, then the starting coordinate frame is equal to World space
  /// (i.e. camera_O_frameA is identity)
  Camera(
    std::unique_ptr<Projection> projection,
    GetterType<CoordinateFrame> frameB_O_frameA_provider = nullptr
  );

  Camera(const Camera&) = delete;
  Camera& operator=(const Camera&) = delete;

  Camera(Camera&&) = default;
  Camera& operator=(Camera&&) = default;

  ~Camera() = default;

  /// Set the camera projection. (Must not be null.)
  void setProjection(std::unique_ptr<Projection> projection);

  /// Get a non-owning const pointer to the camera projection.
  /// This pointer should not be stored by the caller.
  const Projection* projection() const;

  /// Set the functional that defines the starting frame of reference to which the camera is linked.
  void set_frameB_O_frameA_provider(GetterType<CoordinateFrame>);

  /// Get the camera's starting frame, if it is linked to one. Returns std::nullopt iff the
  /// camera is not linked to a starting frame.
  std::optional<CoordinateFrame> startFrame() const;

  /// Get whether the camera is linked to a starting frame of reference. Returns true iff
  /// the camera is linked to a starting frame. If not linked to a starting frame,
  /// then startFrame_O_world is identity.
  bool isLinkedToStartFrame() const;

  /// Get the transformation from World space to the camera's starting frame of reference.
  /// If the camera is linked to a start frame, then this returns the linked frame transformation.
  /// If not linked, then this returns identity.
  glm::mat4 frameB_O_frameA() const;

  const glm::mat4& frameA_O_world() const;
  void set_frameA_O_world(glm::mat4);

  /// Set the matrix defining the camera's position relative to the anatomical frame of reference.
  /// @note This should be a rigid-body matrix (i.e. orthonormal rotational component),
  /// but this constraint is not enforced.
  void set_camera_O_frameB(glm::mat4 camera_O_frameB);

  /// Get the transformation from the camera's anatomical frame of reference to its nominal orientation.
  const glm::mat4& camera_O_frameB() const;

  /// Get the camera's model-view transformation. This is equal to
  /// camera_O_startFrame() * startFrame_O_world().
  glm::mat4 camera_O_world() const;

  /// Get the inverse of the camera's model-view transformation. This is qual to
  /// inverse( camera_O_world() ).
  glm::mat4 world_O_camera() const;

  /// Get the camera's projection transformation.
  glm::mat4 clip_O_camera() const;

  /// Get the inverse of the camera's projection transformation.
  glm::mat4 camera_O_clip() const;

  /// Set the aspect ratio (width/height) of the view associated with this camera.
  /// (The aspect ratio must be positive.)
  void setAspectRatio(float ratio);

  /// Get whether the camera's projection is orthographic.
  bool isOrthographic() const;

  /// Set the camera zoom factor. (Zoom factor must be positive.)
  void setZoom(float factor);

  /// Set the frustum near clip plane distance. (The near distance must be positive and
  /// less than the far distance.)
  void setNearDistance(float d);

  /// Set the frustum far clip plane distance. (The far distance must be positive and
  /// greater than the near distance.)
  void setFarDistance(float d);

  /// Set the default camera field of view (in x and y) for orthographic projections.
  /// (This parameter only affects cameras with orthographic projection.)
  void setDefaultFov(const glm::vec2& fov);

  /// Get the zoom factor.
  float getZoom() const;

  /// Get the frustum angle in radians. Returns 0 for orthographic projections.
  float angle() const;

  /// Get the frustum near plane distance.
  float nearDistance() const;

  /// Get the frustum far plane distance.
  float farDistance() const;

private:
  /// Camera projection (either perspective or orthographic)
  std::unique_ptr<Projection> m_projection;

  /// Functional providing the start frame of the camera relative to World space.
  /// If null, then identity is used for startFrame_O_world.
  GetterType<CoordinateFrame> m_frameB_O_frameA_provider;

  /// Transformation of the camera relative to its start frame.
  /// @note This should be a rigid-body transformation!
  glm::mat4 m_camera_O_frameB;

  glm::mat4 m_frameA_O_world;
};

} // namespace camera

#endif // CAMERA_H
