#include "logic/camera/PerspectiveProjection.h"

#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace
{
constexpr float k_initAngle = glm::pi<float>() / 3.0f;
constexpr float k_minAngle = glm::radians(0.5f);
constexpr float k_maxAngle = glm::radians(120.0f);
} // namespace

PerspectiveProjection::PerspectiveProjection()
  : Projection()
{
}

ProjectionType PerspectiveProjection::type() const
{
  return ProjectionType::Perspective;
}

glm::mat4 PerspectiveProjection::clip_T_camera() const
{
  return glm::perspective(angle(), m_aspectRatio, m_nearDistance, m_farDistance);
}

void PerspectiveProjection::setZoom(float factor)
{
  if (factor > 0.0f)
  {
    m_zoom = glm::clamp(factor, k_initAngle / k_maxAngle, k_initAngle / k_minAngle);
  }
}

float PerspectiveProjection::angle() const
{
  return glm::clamp(k_initAngle / m_zoom, k_minAngle, k_maxAngle);
}
