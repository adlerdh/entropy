#include "logic/camera/PerspectiveProjection.h"

#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace
{

static const float sk_initAngle = glm::pi<float>() / 3.0f;
static const float sk_minAngle = glm::radians(0.5f);
static const float sk_maxAngle = glm::radians(120.0f);

} // namespace

namespace camera
{

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
    m_zoom = glm::clamp(factor, sk_initAngle / sk_maxAngle, sk_initAngle / sk_minAngle);
  }
}

float PerspectiveProjection::angle() const
{
  return glm::clamp(sk_initAngle / m_zoom, sk_minAngle, sk_maxAngle);
}

} // namespace camera
