#ifndef CAMERA_ORTHOGRAPHIC_PROJECTION_H
#define CAMERA_ORTHOGRAPHIC_PROJECTION_H

#include "logic/camera/Projection.h"

namespace camera
{

class OrthographicProjection final : public Projection
{
public:
  explicit OrthographicProjection();
  ~OrthographicProjection() override = default;

  ProjectionType type() const override;

  glm::mat4 clip_T_camera() const override;

  void setZoom(float factor) override;

  float angle() const override;
};

} // namespace camera

#endif // CAMERA_ORTHOGRAPHIC_PROJECTION_H
