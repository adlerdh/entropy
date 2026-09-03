#include "logic/camera/Projection.h"

#include <glm/glm.hpp>
#define GLM_FORCE_RADIANS

#include <cmath>

Projection::Projection()
  : m_aspectRatio(1.0f), m_nearDistance(0.1f), m_farDistance(1000.0f), m_defaultFov(5.0f), m_zoom(1.0f)
{
}

glm::mat4 Projection::camera_T_clip() const
{
  return glm::inverse(clip_T_camera());
}

void Projection::setAspectRatio(float aspect)
{
  if (std::isfinite(aspect) && aspect > 0.0f) {
    m_aspectRatio = aspect;
  }
}

void Projection::setNearDistance(float distance)
{
  if (std::isfinite(distance) && 0.0f < distance && distance < m_farDistance) {
    m_nearDistance = distance;
  }
}

void Projection::setFarDistance(float distance)
{
  if (std::isfinite(distance) && 0.0f < distance && m_nearDistance < distance) {
    m_farDistance = distance;
  }
}

void Projection::setClipDistances(float nearDistanceArg, float farDistanceArg)
{
  if (
    std::isfinite(nearDistanceArg) && std::isfinite(farDistanceArg) && 0.0f < nearDistanceArg &&
    nearDistanceArg < farDistanceArg)
  {
    m_nearDistance = nearDistanceArg;
    m_farDistance = farDistanceArg;
  }
}

float Projection::aspectRatio() const
{
  return m_aspectRatio;
}

float Projection::nearDistance() const
{
  return m_nearDistance;
}

float Projection::farDistance() const
{
  return m_farDistance;
}

void Projection::setDefaultFov(const glm::vec2& fov)
{
  if (!std::isfinite(fov.x) || !std::isfinite(fov.y) || fov.x <= 0.0f || fov.y <= 0.0f) {
    return;
  }

  m_defaultFov = fov;
}

glm::vec2 Projection::defaultFov() const
{
  return m_defaultFov;
}

void Projection::resetZoom()
{
  static constexpr float sk_defaultZoom = 1.0f;
  setZoom(sk_defaultZoom);
}

float Projection::getZoom() const
{
  return m_zoom;
}
