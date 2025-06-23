#pragma once

#include "logic/camera/Projection.h"

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
