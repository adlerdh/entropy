#pragma once

#include "logic/camera/Projection.h"

namespace camera
{

class PerspectiveProjection final : public Projection
{
public:
  explicit PerspectiveProjection();
  ~PerspectiveProjection() override = default;

  ProjectionType type() const override;

  glm::mat4 clip_O_camera() const override;

  void setZoom(float factor) override;

  float angle() const override;
};

} // namespace camera
