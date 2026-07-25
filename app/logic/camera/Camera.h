#pragma once

#include "common/CoordinateFrame.h"
#include "common/PublicTypes.h"
#include "logic/camera/Projection.h"

#include <glm/fwd.hpp>

#include <memory>
#include <optional>

/**
 * @brief Camera that maps World space to OpenGL Clip space through view and projection transforms
 *
 * clip_T_world = clip_T_camera * camera_T_world,
 *
 * where clip_T_camera is a Projection transformation, either orthographic or perspective,
 * and camera_T_world is a rigid-body matrix, sometimes referred to as the View transformation,
 * that maps World to Camera space. Its parts are
 *
 * camera_T_world = camera_T_anatomy * anatomy_T_start * start_T_world,
 * where:
 *
 * i) start_T_world: User manipulations applied to the camera BEFORE the anatomical transformation
 * Currently fixed as the identity, so that Start == World space
 *
 * ii) anatomy_T_start: Anatomical starting frame of reference that is linked
 * to an external callback. This is where axial, coronal, sagittal, and crosshairs-Z/Y/X view
 * orientations are set
 *
 * iii) camera_T_anatomy: This holds user manipulations applied to the camera AFTER
 * the anatomical transformation. This is used for manual user view manipulations
 * (e.g. translation, rotation)
 *
 * Definitions of coordinate spaces:
 * Clip -- Standard OpenGL Clip space (normalized to [-1, 1]^3)
 * Camera -- Space of the view camera's intrinsic reference frame (in physical coordinates)
 * Anatomy -- Anatomical frame of reference of a subject (in physical coordinates)
 * Start -- Starting frame of reference (in physical coordinates)
 * World -- World space, common to all objects of the scene (in physical coordinates)
 *
 * Note: the transformation world_T_subject is the MODEL matrix (M), so the full chain can also be
 * written as clip_T_subject = P * V * M = clip_T_camera * camera_T_world * world_T_subject
 *
 * We can also call "anatomy" space the "crosshairs" space. Then:
 * Suppose we start with a point in image/subject space, and want to go all the way to clip space
 * Here's what happens step-by-step:
 *   M = world_T_subject      Maps from image/subject space to world space (image header
 * transformation) R = crosshairs_T_world   Maps from world space to crosshairs-aligned space
 * (custom rotated axes) V = camera_T_crosshairs  Maps from crosshairs space to camera space
 * (accounts for zoom, pan, etc.) P = clip_T_camera        Maps from camera space to clip space
 * (perspective or orthographic projection)
 *
 * So the full transform
 *   clipPos = P * V * R * M * subjectPos
 * is
 *   clipPos = clip_T_camera * camera_T_crosshairs * crosshairs_T_world * world_T_subject *
 * subjectPos
 */
class Camera
{
public:
  /**
   * @brief Construct a camera with an owned projection and optional linked start-frame provider
   * @param projection Projection used to map Camera space to Clip space
   * @param anatomy_T_start_provider Optional callback that supplies the linked anatomical start frame
   * @throw Throws `DebugException` when `projection` is null
   */
  Camera(std::unique_ptr<Projection> projection, GetterType<CoordinateFrame> anatomy_T_start_provider = nullptr);

  /**
   * @brief Construct a camera with a projection of the requested type
   * @param projType Projection type to create
   * @param anatomy_T_start_provider Optional callback that supplies the linked anatomical start frame
   * @throw Propagates exceptions from projection allocation
   */
  Camera(ProjectionType projType, GetterType<CoordinateFrame> anatomy_T_start_provider = nullptr);

  /**
   * @brief Copy a camera and duplicate its projection parameters
   * @param other Camera to copy
   * @throw Propagates exceptions from projection allocation
   */
  Camera(const Camera& other);
  /**
   * @brief Copy camera pose, provider, and projection parameters
   * @param other Camera to copy
   * @return Reference to this camera
   * @throw Propagates exceptions from projection allocation
   */
  Camera& operator=(const Camera& other);

  /**
   * @brief Destroy the camera and owned projection
   */
  ~Camera() = default;

  /**
   * @brief Replace the camera projection
   * @param projection New projection. Null projections are ignored
   */
  void setProjection(std::unique_ptr<Projection> projection);

  /**
   * @brief Get the camera projection
   * @return Non-owning pointer to the current projection
   */
  const Projection* projection() const;

  /**
   * @brief Set the callback that defines the camera's linked anatomical start frame
   * @param provider Callback returning the current anatomical start frame, or null to unlink the camera
   */
  void set_anatomy_T_start_provider(GetterType<CoordinateFrame>);

  /**
   * @brief Get the callback that defines the camera's linked anatomical start frame
   * @return Start-frame provider callback
   */
  const GetterType<CoordinateFrame>& anatomy_T_start_provider() const;

  /**
   * @brief Get the camera's linked starting frame when one is available
   * @return Start frame from the provider, or `std::nullopt` when the camera is unlinked
   * @throw Propagates exceptions from the start-frame provider
   */
  std::optional<CoordinateFrame> startFrame() const;

  /**
   * @brief Check whether the camera is linked to a start-frame provider
   * @return True when a provider callback is installed
   */
  bool isLinkedToStartFrame() const;

  /**
   * @brief Get the linked anatomy-from-start transform used in the camera transform chain
   * @return Provider frame matrix, or identity when the camera is unlinked
   * @throw Propagates exceptions from the start-frame provider
   */
  glm::mat4 anatomy_T_start() const;

  /**
   * @brief Get the transform from World space to the camera's start space
   * @return Stored start_T_world transform
   */
  const glm::mat4& start_T_world() const;
  /**
   * @brief Set the transform from World space to the camera's start space
   * @param start_T_world Transform to store
   */
  void set_start_T_world(glm::mat4 start_T_world);

  /**
   * @brief Set the transform from Anatomy space to Camera space
   * @param camera_T_anatomy Rigid affine transform to store
   */
  void set_camera_T_anatomy(glm::mat4 camera_T_anatomy);

  /**
   * @brief Get the transform from Anatomy space to Camera space
   * @return Stored camera_T_anatomy transform
   */
  const glm::mat4& camera_T_anatomy() const;

  /**
   * @brief Get the full transform from World space to Camera space
   * @return `camera_T_anatomy * anatomy_T_start * start_T_world`
   * @throw Propagates exceptions from the start-frame provider
   */
  glm::mat4 camera_T_world() const;

  /**
   * @brief Get the full transform from Camera space to World space
   * @return Inverse of `camera_T_world()`
   * @throw Propagates exceptions from the start-frame provider
   */
  glm::mat4 world_T_camera() const;

  /**
   * @brief Get the projection transform from Camera space to Clip space
   * @return Current projection matrix
   * @throw Propagates exceptions from the projection implementation
   */
  glm::mat4 clip_T_camera() const;

  /**
   * @brief Get the inverse projection transform from Clip space to Camera space
   * @return Inverse projection matrix
   * @throw Propagates exceptions from the projection implementation
   */
  glm::mat4 camera_T_clip() const;

  /**
   * @brief Set the view aspect ratio
   * @param ratio Width divided by height. Non-positive values are ignored
   */
  void setAspectRatio(float ratio);
  /**
   * @brief Get the view aspect ratio
   * @return Width divided by height
   */
  float aspectRatio() const;

  /**
   * @brief Check whether the current projection is orthographic
   * @return True for orthographic projection
   */
  bool isOrthographic() const;

  /**
   * @brief Set the camera zoom factor
   * @param factor Zoom factor in the accepted camera range. Out-of-range values are ignored
   */
  void setZoom(float factor);

  /**
   * @brief Get the camera zoom factor
   * @return Current zoom factor
   */
  float getZoom() const;

  /**
   * @brief Set the default field of view used by the projection
   * @param fov Default horizontal and vertical field of view values
   */
  void setDefaultFov(const glm::vec2& fov);

  /**
   * @brief Get the perspective view angle
   * @return View angle in radians, or zero for orthographic projections
   */
  float angle() const;

  /**
   * @brief Set the near clipping plane distance from the camera origin
   * @param d Near distance. Invalid values are ignored by the projection
   */
  void setNearDistance(float d);

  /**
   * @brief Set the far clipping plane distance from the camera origin
   * @param d Far distance. Invalid values are ignored by the projection
   */
  void setFarDistance(float d);

  /**
   * @brief Get the near clipping plane distance
   * @return Near distance from the camera origin
   */
  float nearDistance() const;

  /**
   * @brief Get the far clipping plane distance
   * @return Far distance from the camera origin
   */
  float farDistance() const;

private:
  /**
   * @brief Reserved copy-swap helper
   * @param other Camera whose contents would be swapped
   * @throw Not implemented
   */
  void swap(const Camera& other);

  /** @brief Owned projection, either perspective or orthographic */
  std::unique_ptr<Projection> m_projection;

  /** @brief Callback providing the start frame relative to World space, or null for identity */
  GetterType<CoordinateFrame> m_anatomy_T_start_provider;

  /** @brief Rigid transform from Anatomy space to Camera space */
  glm::mat4 m_camera_T_anatomy;

  /** @brief Transform from World space to camera Start space */
  glm::mat4 m_start_T_world;
};
