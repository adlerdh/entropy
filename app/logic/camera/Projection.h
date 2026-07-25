#pragma once

#include "logic/camera/CameraTypes.h"

#include <glm/fwd.hpp>
#include <glm/vec2.hpp>

/**
 * @brief Base class for camera projections from Camera space to OpenGL Clip space
 */
class Projection
{
public:
  /**
   * @brief Construct a projection with default aspect, clipping, field of view, and zoom values
   */
  explicit Projection();
  /**
   * @brief Destroy the projection
   */
  virtual ~Projection() = default;

  /**
   * @brief Get the concrete projection type
   * @return Projection type
   */
  virtual ProjectionType type() const = 0;

  /**
   * @brief Get the projection transform from Camera space to Clip space
   * @return Projection matrix
   */
  virtual glm::mat4 clip_T_camera() const = 0;

  /**
   * @brief Set the projection zoom factor
   * @param factor Zoom factor, where 1.0 is the default
   */
  virtual void setZoom(float factor) = 0;

  /**
   * @brief Get the projection view angle
   * @return View angle in radians, or zero for orthographic projections
   */
  virtual float angle() const = 0;

  /**
   * @brief Get the projection zoom factor
   * @return Current zoom factor
   */
  float getZoom() const;

  /**
   * @brief Reset the zoom factor to the default value
   */
  void resetZoom();

  /**
   * @brief Get the inverse projection transform from Clip space to Camera space
   * @return Inverse projection matrix
   */
  glm::mat4 camera_T_clip() const;

  /**
   * @brief Set the view aspect ratio
   * @param aspect Width divided by height. Non-positive values are ignored
   */
  void setAspectRatio(float aspect);
  /**
   * @brief Get the view aspect ratio
   * @return Width divided by height
   */
  float aspectRatio() const;

  /**
   * @brief Set the near clipping plane distance from the Camera origin
   * @param distance Near distance. Invalid values are ignored
   */
  void setNearDistance(float distance);
  /**
   * @brief Get the near clipping plane distance from the Camera origin
   * @return Near clipping distance
   */
  float nearDistance() const;

  /**
   * @brief Set the far clipping plane distance from the Camera origin
   * @param distance Far distance. Invalid values are ignored
   */
  void setFarDistance(float distance);
  /**
   * @brief Get the far clipping plane distance from the Camera origin
   * @return Far clipping distance
   */
  float farDistance() const;

  /**
   * @brief Set the default field of view of the projection
   * @param defaultFov Horizontal and vertical default field-of-view extents
   */
  void setDefaultFov(const glm::vec2& defaultFov);
  /**
   * @brief Get the default field of view of the projection
   * @return Horizontal and vertical default field-of-view extents
   */
  glm::vec2 defaultFov() const;

protected:
  /** @brief View aspect ratio as width divided by height */
  float m_aspectRatio;

  /** @brief Near clipping plane distance from the Camera origin */
  float m_nearDistance;
  /** @brief Far clipping plane distance from the Camera origin */
  float m_farDistance;

  /** @brief Default horizontal and vertical field-of-view extents */
  glm::vec2 m_defaultFov;

  /** @brief Projection zoom factor */
  float m_zoom;
};
