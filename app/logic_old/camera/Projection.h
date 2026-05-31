#ifndef CAMERA_PROJECTION_H
#define CAMERA_PROJECTION_H

#include "logic/camera/CameraTypes.h"

#include <glm/fwd.hpp>
#include <glm/vec2.hpp>

namespace camera
{

class Projection
{
public:
  explicit Projection();
  virtual ~Projection() = default;

  virtual ProjectionType type() const = 0;

  /// Get the projection transformation (i.e. from Camera to Clip space)
  virtual glm::mat4 clip_O_camera() const = 0;

  /// Set the zoom factor, assuming that 1.0 is the default
  virtual void setZoom(float factor) = 0;

  /// Get the angle of view
  virtual float angle() const = 0;

  /// Get the zoom factor
  float getZoom() const;

  /// Reset the zoom factor
  void resetZoom();

  /// Get the inverse projection transformation (i.e. from Clip to Camera space)
  glm::mat4 camera_O_clip() const;

  /// Set/get the view aspect ratio (width/height)
  void setAspectRatio(float ratio);
  float aspectRatio() const;

  /// Set/get the near clipping plane distance from the Camera origin
  void setNearDistance(float distance);
  float nearDistance() const;

  /// Set/get the far clipping plane distance from the Camera origin
  void setFarDistance(float distance);
  float farDistance() const;

  /// Set/get the default field of view of the projection
  void setDefaultFov(const glm::vec2& defaultFov);
  glm::vec2 defaultFov() const;

protected:
  float m_aspectRatio;

  float m_nearDistance;
  float m_farDistance;

  glm::vec2 m_defaultFov;

  float m_zoom;
};

} // namespace camera

#endif // CAMERA_PROJECTION_H
