#ifndef CAMERA_H
#define CAMERA_H

#include "common/CoordinateFrame.h"
#include "common/PublicTypes.h"
#include "logic/camera/Projection.h"

#include <glm/fwd.hpp>

#include <memory>
#include <optional>

namespace camera
{

/**
 * @brief Camera class that manages the mapping World space to OpenGL Clip space via a sequence of
 * transformation matrices:
 *
 * clip_T_world = clip_T_camera * camera_T_world,
 *
 * where clip_T_camera is a projection transformation, either orthogonal or perpspective,
 * and camera_T_world is a rigid-body matrix, sometimes referred to as the View transformation that
 * maps World to Camera space. Its parts are
 *
 * camera_T_world = camera_T_anatomy * anatomy_T_start * start_T_world,
 * where:
 *
 *     i) start_T_world: User manipulations applied to the camera BEFORE the anatomical transformation.
 *
 *    ii) anatomy_T_start: Anatomical starting frame of reference that is linked to an external callback.
 *        This is where axial, coronal, sagittal, and crosshairs-Z/Y/X view orientations are set.
 *
 *    iii) camera_T_anatomy: User manipulations applied to the camera AFTER the anatomical transformation.
 *         This is used for manual user view manipulations (e.g. translation, rotation).
 *
 * Definitions of coordinate spaces:
 * Clip -- Standard OpenGL Clip space (normalized to [-1, 1]^3)
 * Camera -- Space of the view camera's intrinsic reference frame (in physical coordinates)
 * Anatomy -- Anatomical frame of reference of a subject (in physical coordinates)
 * Start -- Starting frame of reference (in physical coordinates)
 * World -- World space, common to all objects of the scene (in physical coordinates)
 */
class Camera
{
public:
  /// Construct a camera with a projection (either orthographic or perspective) and a functional that
  /// returns the transformation from the camera's start coordinate frame to the camera's anatomical
  /// coordinate frame. If no functional is supplied, then the anatomical coordinate frame is equal
  /// to the Start space. (i.e. anatomy_T_start is identity)
  Camera(
    std::unique_ptr<Projection> projection,
    GetterType<CoordinateFrame> anatomy_T_start_provider = nullptr
  );

  Camera(ProjectionType projType, GetterType<CoordinateFrame> anatomy_T_start_provider = nullptr);

  Camera(const Camera&);
  const Camera& operator=(const Camera&);

  ~Camera() = default;

  /// Set the camera projection. (Must not be null.)
  void setProjection(std::unique_ptr<Projection> projection);

  /// Get a non-owning const pointer to the camera projection.
  /// This pointer should not be stored by the caller.
  const Projection* projection() const;

  /// Set the functional that defines the starting frame of reference to which the camera is linked.
  void set_anatomy_T_start_provider(GetterType<CoordinateFrame>);

  /// Get the functional that defines the starting frame of reference to which the camera is linked.
  const GetterType<CoordinateFrame>& anatomy_T_start_provider() const;

  /// Get the camera's starting frame, if it is linked to one. Returns std::nullopt iff the
  /// camera is not linked to a starting frame.
  std::optional<CoordinateFrame> startFrame() const;

  /// Get whether the camera is linked to a starting frame of reference. Returns true iff
  /// the camera is linked to a starting frame. If not linked to a starting frame,
  /// then start_T_world is identity.
  bool isLinkedToStartFrame() const;

  /// Get the transformation from World space to the camera's starting frame of reference.
  /// If the camera is linked to a start frame, then this returns the linked frame transformation.
  /// If not linked, then this returns identity.
  glm::mat4 anatomy_T_start() const;

  const glm::mat4& start_T_world() const;
  void set_start_T_world(glm::mat4 start_T_world);

  /// Set the matrix defining the camera's position relative to the anatomical frame of reference.
  /// @note This must be a rigid-body matrix (i.e. orthonormal rotational component) with determinant 1.
  void set_camera_T_anatomy(glm::mat4 camera_T_anatomy);

  /// Get the transformation from the camera's anatomical frame of reference to its nominal orientation.
  const glm::mat4& camera_T_anatomy() const;

  /// Get the camera's model-view transformation. This is equal to
  /// camera_T_startFrame() * startFrame_T_world().
  glm::mat4 camera_T_world() const;

  /// Get the inverse of the camera's model-view transformation. This is qual to
  /// inverse( camera_T_world() ).
  glm::mat4 world_T_camera() const;

  /// Get the camera's projection transformation.
  glm::mat4 clip_T_camera() const;

  /// Get the inverse of the camera's projection transformation.
  glm::mat4 camera_T_clip() const;

  /// Set the aspect ratio (width/height) of the view associated with this camera.
  /// (The aspect ratio must be positive.)
  void setAspectRatio(float ratio);
  float aspectRatio() const;

  /// Get whether the camera's projection is orthographic.
  bool isOrthographic() const;

  /// Set the camera zoom factor. (Zoom factor must be positive.)
  void setZoom(float factor);

  /// Get the zoom factor.
  float getZoom() const;

  /// Set the default camera field of view (in x and y) for orthographic projections.
  /// (This parameter only affects cameras with orthographic projection.)
  void setDefaultFov(const glm::vec2& fov);

  /// Get the frustum angle in radians. Returns 0 for orthographic projections.
  float angle() const;

  /// Set the frustum near clip plane distance. (The near distance must be positive and
  /// less than the far distance.)
  void setNearDistance(float d);

  /// Set the frustum far clip plane distance. (The far distance must be positive and
  /// greater than the near distance.)
  void setFarDistance(float d);

  /// Get the frustum near plane distance.
  float nearDistance() const;

  /// Get the frustum far plane distance.
  float farDistance() const;

private:
  void swap(const Camera& other);

  /// Camera projection (either perspective or orthographic)
  std::unique_ptr<Projection> m_projection;

  /// Functional providing the start frame of the camera relative to World space.
  /// If null, then identity is used for anatomy_T_start.
  GetterType<CoordinateFrame> m_anatomy_T_start_provider;

  /// Transformation of the camera relative to its start frame.
  /// @note This should be a rigid-body transformation!
  glm::mat4 m_camera_T_anatomy;

  glm::mat4 m_start_T_world;
};

} // namespace camera

#endif // CAMERA_H
