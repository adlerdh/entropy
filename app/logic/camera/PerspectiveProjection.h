#pragma once

#include "logic/camera/Projection.h"

class PerspectiveProjection final : public Projection
{
public:
  explicit PerspectiveProjection() = default;
  ~PerspectiveProjection() override = default;

  ProjectionType type() const override;

  glm::mat4 clip_T_camera() const override;

  void setZoom(float factor) override;

  float angle() const override;
};
