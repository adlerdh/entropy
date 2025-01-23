#include "logic/camera/Camera.h"
#include "logic/camera/CameraHelpers.h"

#include "common/HZeeException.hpp"

#include "rendering/utility/math/MathUtility.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/matrix_transform.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/matrix_cross_product.hpp>
#include <glm/gtx/norm.hpp>
#include <glm/gtx/orthonormalize.hpp>
#include <glm/gtx/string_cast.hpp>
#include <glm/gtx/transform.hpp>

namespace
{

static const glm::mat4 sk_ident(1.0f);

} // namespace

namespace camera
{

Camera::Camera(
  std::unique_ptr<Projection> projection, GetterType<CoordinateFrame> frameB_O_frameA_provider
)
  : m_projection(std::move(projection))
  , m_frameB_O_frameA_provider(frameB_O_frameA_provider)
  , m_camera_O_frameB(1.0f)
  , m_frameA_O_world(1.0f)
{
  if (!m_projection)
  {
    throw_debug("Cannot construct Camera with null Projection");
  }
}

void Camera::setProjection(std::unique_ptr<Projection> projection)
{
  if (projection)
  {
    m_projection = std::move(projection);
  }
}

const Projection* Camera::projection() const
{
  return m_projection.get();
}

void Camera::set_frameB_O_frameA_provider(GetterType<CoordinateFrame> provider)
{
  m_frameB_O_frameA_provider = provider;
}

std::optional<CoordinateFrame> Camera::startFrame() const
{
  if (m_frameB_O_frameA_provider)
  {
    return m_frameB_O_frameA_provider();
  }
  else
  {
    return std::nullopt;
  }
}

bool Camera::isLinkedToStartFrame() const
{
  return (m_frameB_O_frameA_provider ? true : false);
}

void Camera::set_camera_O_frameB(glm::mat4 camera_O_frameB)
{
  /// @todo Check that this is rigid-body
  m_camera_O_frameB = std::move(camera_O_frameB);
}

const glm::mat4& Camera::camera_O_frameB() const
{
  return m_camera_O_frameB;
}

glm::mat4 Camera::frameB_O_frameA() const
{
  return (m_frameB_O_frameA_provider ? m_frameB_O_frameA_provider().frame_O_world() : sk_ident);
}

void Camera::set_frameA_O_world(glm::mat4 frameA_O_world)
{
  m_frameA_O_world = std::move(frameA_O_world);
}

const glm::mat4& Camera::frameA_O_world() const
{
  return m_frameA_O_world;
}

glm::mat4 Camera::camera_O_world() const
{
  return camera_O_frameB() * frameB_O_frameA() * frameA_O_world();
}

glm::mat4 Camera::world_O_camera() const
{
  return glm::inverse(camera_O_world());
}

glm::mat4 Camera::clip_O_camera() const
{
  return m_projection->clip_O_camera();
}

glm::mat4 Camera::camera_O_clip() const
{
  return m_projection->camera_O_clip();
}

void Camera::setAspectRatio(float ratio)
{
  if (ratio > 0.0f)
  {
    m_projection->setAspectRatio(ratio);
  }
}

bool Camera::isOrthographic() const
{
  return (ProjectionType::Orthographic == m_projection->type());
}

void Camera::setZoom(float factor)
{
  if (factor > 0.0f)
  {
    m_projection->setZoom(factor);
  }
}

void Camera::setNearDistance(float dist)
{
  m_projection->setNearDistance(dist);
}

void Camera::setFarDistance(float dist)
{
  m_projection->setFarDistance(dist);
}

void Camera::setDefaultFov(const glm::vec2& fov)
{
  m_projection->setDefaultFov(fov);
}

float Camera::getZoom() const
{
  return m_projection->getZoom();
}

float Camera::angle() const
{
  return m_projection->angle();
}

float Camera::nearDistance() const
{
  return m_projection->nearDistance();
}

float Camera::farDistance() const
{
  return m_projection->farDistance();
}

} // namespace camera
