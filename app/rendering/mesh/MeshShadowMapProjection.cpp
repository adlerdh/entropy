#include "rendering/mesh/MeshShadowMapProjection.h"

#include <glm/common.hpp>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/geometric.hpp>
#include <glm/vec4.hpp>

#include <algorithm>
#include <array>
#include <cmath>

namespace rendering::mesh
{

namespace
{

constexpr float k_minSceneRadiusMm = 0.5f;
constexpr float k_minOrthoHalfExtentMm = 0.5f;
constexpr float k_depthMarginFraction = 0.25f;
constexpr float k_minDepthRangeMm = 1.0f;
constexpr float k_minLightDirectionLength2 = 1.0e-8f;

std::array<glm::vec3, 8> boundsCorners(const MeshBounds& bounds) noexcept
{
  return {
    glm::vec3{bounds.min.x, bounds.min.y, bounds.min.z},
    glm::vec3{bounds.max.x, bounds.min.y, bounds.min.z},
    glm::vec3{bounds.min.x, bounds.max.y, bounds.min.z},
    glm::vec3{bounds.max.x, bounds.max.y, bounds.min.z},
    glm::vec3{bounds.min.x, bounds.min.y, bounds.max.z},
    glm::vec3{bounds.max.x, bounds.min.y, bounds.max.z},
    glm::vec3{bounds.min.x, bounds.max.y, bounds.max.z},
    glm::vec3{bounds.max.x, bounds.max.y, bounds.max.z}};
}

glm::vec3 lightUpVector(const glm::vec3& lightDirection) noexcept
{
  const glm::vec3 candidate =
    std::abs(lightDirection.z) < 0.9f ? glm::vec3{0.0f, 0.0f, 1.0f} : glm::vec3{0.0f, 1.0f, 0.0f};
  return glm::normalize(candidate - lightDirection * glm::dot(candidate, lightDirection));
}

} // namespace

std::optional<MeshShadowMapProjection> meshShadowMapProjectionForBounds(
  const MeshBounds& bounds,
  const glm::vec3& lightDirectionWorld) noexcept
{
  if (!isFinite(bounds.min) || !isFinite(bounds.max) || glm::any(glm::lessThan(bounds.max, bounds.min))) {
    return std::nullopt;
  }

  if (!isFinite(lightDirectionWorld) || glm::dot(lightDirectionWorld, lightDirectionWorld) < k_minLightDirectionLength2)
  {
    return std::nullopt;
  }

  const glm::vec3 lightDirection = glm::normalize(lightDirectionWorld);
  const glm::vec3 sceneCenter = center(bounds);
  const float sceneRadius = std::max(glm::length(diagonal(bounds)) * 0.5f, k_minSceneRadiusMm);
  const float margin = std::max(sceneRadius * k_depthMarginFraction, k_minDepthRangeMm * 0.5f);
  const glm::vec3 eye = sceneCenter + lightDirection * (sceneRadius + margin);
  const glm::mat4 lightView_T_world = glm::lookAt(eye, sceneCenter, lightUpVector(lightDirection));

  glm::vec3 minLight{std::numeric_limits<float>::max()};
  glm::vec3 maxLight{std::numeric_limits<float>::lowest()};
  for (const glm::vec3& corner : boundsCorners(bounds)) {
    const glm::vec3 lightCorner = glm::vec3{lightView_T_world * glm::vec4{corner, 1.0f}};
    minLight = glm::min(minLight, lightCorner);
    maxLight = glm::max(maxLight, lightCorner);
  }

  const float halfWidth = std::max((maxLight.x - minLight.x) * 0.5f + margin, k_minOrthoHalfExtentMm);
  const float halfHeight = std::max((maxLight.y - minLight.y) * 0.5f + margin, k_minOrthoHalfExtentMm);
  const float centerX = (minLight.x + maxLight.x) * 0.5f;
  const float centerY = (minLight.y + maxLight.y) * 0.5f;

  const float nearDistance = std::max(0.001f, -maxLight.z - margin);
  const float farDistance = std::max(nearDistance + k_minDepthRangeMm, -minLight.z + margin);
  const glm::mat4 lightClip_T_lightView = glm::ortho(
    centerX - halfWidth,
    centerX + halfWidth,
    centerY - halfHeight,
    centerY + halfHeight,
    nearDistance,
    farDistance);

  return MeshShadowMapProjection{
    .lightClip_T_world = lightClip_T_lightView * lightView_T_world,
    .lightDirectionWorld = lightDirection};
}

} // namespace rendering::mesh
