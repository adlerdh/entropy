#include "logic/camera/Camera2DControls.h"

#include "logic/camera/Camera.h"
#include "logic/camera/CameraHelpers.h"

#include <glm/geometric.hpp>

#include <algorithm>
#include <cmath>

namespace
{

constexpr float k_scrollZoomScale = 0.01f;
constexpr float k_interactionTolerance = 1.0e-6f;
constexpr float k_maxZoomExponent = 20.0f;

bool finite(const glm::vec2& value)
{
  return std::isfinite(value.x) && std::isfinite(value.y);
}

bool finite(const glm::vec3& value)
{
  return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

float exponentialZoomFactor(float exponent) noexcept
{
  return std::isfinite(exponent) ? std::exp(std::clamp(exponent, -k_maxZoomExponent, k_maxZoomExponent)) : 1.0f;
}

} // namespace

namespace camera2d
{

float dragZoomFactor(float windowClipDeltaY) noexcept
{
  return exponentialZoomFactor(windowClipDeltaY);
}

float scrollZoomFactor(float scrollDeltaY) noexcept
{
  return exponentialZoomFactor(k_scrollZoomScale * scrollDeltaY);
}

std::optional<glm::vec2> interactionPivotNdc(const Camera& camera, const std::optional<glm::vec3>& worldPivot)
{
  if (!worldPivot) {
    return glm::vec2{0.0f};
  }
  if (!finite(*worldPivot)) {
    return std::nullopt;
  }

  const glm::vec2 pivot{helper::ndc_T_world(camera, *worldPivot)};
  return finite(pivot) ? std::optional{pivot} : std::nullopt;
}

Controller::Controller(Camera& camera) : m_camera(camera) {}

std::optional<glm::vec3>
Controller::pan(const glm::vec2& ndcOldPos, const glm::vec2& ndcNewPos, const glm::vec3& worldAnchor)
{
  if (
    !finite(ndcOldPos) || !finite(ndcNewPos) || !finite(worldAnchor) ||
    glm::length(ndcNewPos - ndcOldPos) < k_interactionTolerance)
  {
    return std::nullopt;
  }

  const glm::vec3 oldOrigin = helper::worldOrigin(m_camera);
  helper::panRelativeToWorldPosition(m_camera, ndcOldPos, ndcNewPos, worldAnchor);
  const glm::vec3 translation = helper::worldOrigin(m_camera) - oldOrigin;
  return finite(translation) && glm::length(translation) >= k_interactionTolerance ? std::optional{translation}
                                                                                   : std::nullopt;
}

bool Controller::translateWorld(const glm::vec3& worldTranslation)
{
  if (!finite(worldTranslation) || glm::length(worldTranslation) < k_interactionTolerance) {
    return false;
  }

  helper::setCameraOrigin(m_camera, helper::worldOrigin(m_camera) + worldTranslation);
  return true;
}

std::optional<float>
Controller::rotateFromPointer(const glm::vec2& ndcOldPos, const glm::vec2& ndcNewPos, const glm::vec2& ndcPivot)
{
  if (!finite(ndcOldPos) || !finite(ndcNewPos) || !finite(ndcPivot)) {
    return std::nullopt;
  }

  const glm::vec2 oldVector = ndcOldPos - ndcPivot;
  const glm::vec2 newVector = ndcNewPos - ndcPivot;
  if (glm::length(oldVector) < k_interactionTolerance || glm::length(newVector) < k_interactionTolerance) {
    return std::nullopt;
  }

  const float cross = oldVector.x * newVector.y - oldVector.y * newVector.x;
  const float angle = std::atan2(cross, glm::dot(oldVector, newVector));
  return rotate(angle, ndcPivot) ? std::optional{angle} : std::nullopt;
}

bool Controller::rotate(float angleRadians, const glm::vec2& ndcPivot)
{
  if (!std::isfinite(angleRadians) || !finite(ndcPivot) || std::abs(angleRadians) < k_interactionTolerance) {
    return false;
  }

  helper::rotateInPlane(m_camera, angleRadians, ndcPivot);
  return true;
}

bool Controller::zoom(float factor, const glm::vec2& ndcPivot)
{
  if (!std::isfinite(factor) || factor <= 0.0f || !finite(ndcPivot)) {
    return false;
  }

  const float oldZoom = m_camera.getZoom();
  helper::zoomNdc(m_camera, factor, ndcPivot);
  return m_camera.getZoom() != oldZoom;
}

} // namespace camera2d
