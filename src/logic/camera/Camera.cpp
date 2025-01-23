#include "logic/camera/Camera.h"
#include "logic/camera/CameraHelpers.h"
#include "logic/camera/OrthogonalProjection.h"
#include "logic/camera/PerspectiveProjection.h"

#include "common/Exception.hpp"

#include "logic/camera/MathUtility.h"

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
  std::unique_ptr<Projection> projection, GetterType<CoordinateFrame> anatomy_T_start_provider
)
  : m_projection(std::move(projection))
  , m_anatomy_T_start_provider(anatomy_T_start_provider)
  , m_camera_T_anatomy(1.0f)
  , m_start_T_world(1.0f)
{
  if (!m_projection)
  {
    throw_debug("Cannot construct Camera with null Projection")
  }
}

Camera::Camera(ProjectionType projType, GetterType<CoordinateFrame> anatomy_T_start_provider)
  : m_anatomy_T_start_provider(anatomy_T_start_provider)
  , m_camera_T_anatomy(1.0f)
  , m_start_T_world(1.0f)
{
  switch (projType)
  {
  case ProjectionType::Orthographic:
  {
    m_projection = std::make_unique<OrthographicProjection>();
    break;
  }
  case ProjectionType::Perspective:
  {
    m_projection = std::make_unique<PerspectiveProjection>();
    break;
  }
  }
}

Camera::Camera(const Camera& other)
{
  this->operator=(other);
}

const Camera& Camera::operator=(const Camera& other)
{
  m_anatomy_T_start_provider = other.anatomy_T_start_provider();
  m_camera_T_anatomy = other.camera_T_anatomy();
  m_start_T_world = other.start_T_world();

  m_projection.reset();

  switch (other.projection()->type())
  {
  case ProjectionType::Orthographic:
  {
    m_projection = std::make_unique<OrthographicProjection>();
    break;
  }
  case ProjectionType::Perspective:
  {
    m_projection = std::make_unique<PerspectiveProjection>();
    break;
  }
  }

  m_projection->setAspectRatio(other.projection()->aspectRatio());
  m_projection->setDefaultFov(other.projection()->defaultFov());
  m_projection->setNearDistance(other.projection()->nearDistance());
  m_projection->setFarDistance(other.projection()->farDistance());
  m_projection->setZoom(other.projection()->getZoom());

  return *this;
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

void Camera::set_anatomy_T_start_provider(GetterType<CoordinateFrame> provider)
{
  m_anatomy_T_start_provider = provider;
}

const GetterType<CoordinateFrame>& Camera::anatomy_T_start_provider() const
{
  return m_anatomy_T_start_provider;
}

std::optional<CoordinateFrame> Camera::startFrame() const
{
  if (m_anatomy_T_start_provider)
  {
    return m_anatomy_T_start_provider();
  }
  else
  {
    return std::nullopt;
  }
}

bool Camera::isLinkedToStartFrame() const
{
  return (m_anatomy_T_start_provider ? true : false);
}

void Camera::set_camera_T_anatomy(glm::mat4 M)
{
  static constexpr float EPS = 1.0e-3f;

  // Check that this is a rigid-body transformation that preserves the
  // right-handed coordinate system (i.e. determinant must equal 1):
  const float det = glm::determinant(glm::mat3{M});

  if (det <= 0.0f || std::abs(det - 1.0f) > EPS)
  {
    spdlog::debug(
      "Cannot set camera_T_anatomy to {} because it is non-rigid; 3x3 determinant = {}",
      glm::to_string(M),
      det
    );
    return;
  }

  if (glm::epsilonNotEqual(M[0][3], 0.0f, EPS) || glm::epsilonNotEqual(M[1][3], 0.0f, EPS) || glm::epsilonNotEqual(M[2][3], 0.0f, EPS) || glm::epsilonNotEqual(M[3][3], 1.0f, EPS))
  {
    spdlog::debug("Cannot set camera_T_anatomy to {} because it is not affine", glm::to_string(M));
    return;
  }

  m_camera_T_anatomy = std::move(M);
}

const glm::mat4& Camera::camera_T_anatomy() const
{
  return m_camera_T_anatomy;
}

glm::mat4 Camera::anatomy_T_start() const
{
  return (m_anatomy_T_start_provider ? m_anatomy_T_start_provider().frame_T_world() : sk_ident);
}

void Camera::set_start_T_world(glm::mat4 frameA_T_world)
{
  m_start_T_world = std::move(frameA_T_world);
}

const glm::mat4& Camera::start_T_world() const
{
  return m_start_T_world;
}

glm::mat4 Camera::camera_T_world() const
{
  return camera_T_anatomy() * anatomy_T_start() * start_T_world();
}

glm::mat4 Camera::world_T_camera() const
{
  return glm::inverse(camera_T_world());
}

glm::mat4 Camera::clip_T_camera() const
{
  return m_projection->clip_T_camera();
}

glm::mat4 Camera::camera_T_clip() const
{
  return m_projection->camera_T_clip();
}

void Camera::setAspectRatio(float ratio)
{
  if (ratio > 0.0f)
  {
    m_projection->setAspectRatio(ratio);
  }
}

float Camera::aspectRatio() const
{
  return m_projection->aspectRatio();
}

bool Camera::isOrthographic() const
{
  return (ProjectionType::Orthographic == m_projection->type());
}

void Camera::setZoom(float factor)
{
  static constexpr float sk_minZoom = 0.001f;
  static constexpr float sk_maxZoom = 1000.0f;

  if (sk_minZoom <= factor && factor <= sk_maxZoom)
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
