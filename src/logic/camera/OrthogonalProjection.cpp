#include "logic/camera/OrthogonalProjection.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <utility>

namespace
{

static constexpr std::pair<float, float> sk_minMaxZoomFactors(0.001f, 1000.0f);

glm::vec2 computeCameraFov(const glm::vec2& minFov, float aspectRatio, float zoom)
{
  glm::vec2 fov;

  if ((minFov.x / minFov.y) < aspectRatio)
  {
    fov.x = minFov.y * aspectRatio;
    fov.y = minFov.y;
  }
  else
  {
    fov.x = minFov.x;
    fov.y = minFov.x / aspectRatio;
  }

  return fov / zoom;
}

} // namespace

namespace camera
{

OrthographicProjection::OrthographicProjection()
  : Projection()
{
}

ProjectionType OrthographicProjection::type() const
{
  return ProjectionType::Orthographic;
}

glm::mat4 OrthographicProjection::clip_T_camera() const
{
  const glm::vec2 focalPlaneFov = computeCameraFov(m_defaultFov, m_aspectRatio, m_zoom);

  return glm::ortho(
    -0.5f * focalPlaneFov.x,
    0.5f * focalPlaneFov.x,
    -0.5f * focalPlaneFov.y,
    0.5f * focalPlaneFov.y,
    m_nearDistance,
    m_farDistance
  );
}

void OrthographicProjection::setZoom(float factor)
{
  if (factor > 0.0f)
  {
    m_zoom = glm::clamp(factor, sk_minMaxZoomFactors.first, sk_minMaxZoomFactors.second);
  }
}

float OrthographicProjection::angle() const
{
  // The angle of view for an orthographic projection is zero.
  return 0.0f;
}

} // namespace camera
