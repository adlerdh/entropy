#include "rendering/mesh/MeshMaterial.h"

#include "rendering/mesh/MeshBounds.h"

#include <glm/common.hpp>

#include <cmath>

namespace rendering::mesh
{

namespace
{

glm::vec4 finiteColorOrFallback(const glm::vec4& color, const glm::vec4& fallbackColor) noexcept
{
  return isFinite(color) ? color : fallbackColor;
}

float finiteValueOrFallback(const float value, const float fallback) noexcept
{
  return std::isfinite(value) ? value : fallback;
}

} // namespace

MeshMaterial sanitizedMaterial(const MeshMaterial& material, const glm::vec4& fallbackColor) noexcept
{
  MeshMaterial sanitized = material;
  sanitized.baseColor =
    glm::clamp(finiteColorOrFallback(material.baseColor, fallbackColor), glm::vec4{0.0f}, glm::vec4{1.0f});
  sanitized.metallic = glm::clamp(finiteValueOrFallback(material.metallic, 0.25f), 0.0f, 1.0f);
  sanitized.roughness = glm::clamp(finiteValueOrFallback(material.roughness, 0.5f), 0.001f, 1.0f);
  sanitized.ambientOcclusion = glm::clamp(finiteValueOrFallback(material.ambientOcclusion, 1.0f), 0.0f, 1.0f);
  sanitized.rimOpacityStrength = glm::clamp(finiteValueOrFallback(material.rimOpacityStrength, 1.0f), 0.0f, 1.0f);
  sanitized.rimEmissionStrength = glm::max(finiteValueOrFallback(material.rimEmissionStrength, 1.0f), 0.0f);
  sanitized.rimPower = glm::max(finiteValueOrFallback(material.rimPower, 2.0f), 0.001f);
  return sanitized;
}

} // namespace rendering::mesh
