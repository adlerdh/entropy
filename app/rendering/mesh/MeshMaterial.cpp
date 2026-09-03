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
  sanitized.triangleEdgeColor = glm::clamp(
    isFinite(glm::vec4{material.triangleEdgeColor, 1.0f}) ? material.triangleEdgeColor : glm::vec3{0.0f},
    glm::vec3{0.0f},
    glm::vec3{1.0f});
  sanitized.metallic = glm::clamp(finiteValueOrFallback(material.metallic, 0.2f), 0.0f, 1.0f);
  sanitized.roughness = glm::clamp(finiteValueOrFallback(material.roughness, 0.3f), 0.001f, 1.0f);
  sanitized.ambientOcclusion = glm::clamp(finiteValueOrFallback(material.ambientOcclusion, 1.0f), 0.0f, 1.0f);
  sanitized.rimOpacityStrength = glm::clamp(finiteValueOrFallback(material.rimOpacityStrength, 1.0f), 0.0f, 1.0f);
  sanitized.rimEmissionStrength = glm::max(finiteValueOrFallback(material.rimEmissionStrength, 1.0f), 0.0f);
  sanitized.rimPower = glm::max(finiteValueOrFallback(material.rimPower, 2.0f), 0.001f);
  return sanitized;
}

MeshMaterial meshMaterialForSurface(const glm::vec4& baseColor, const MeshSurfaceMaterialSettings& settings) noexcept
{
  return sanitizedMaterial(
    MeshMaterial{
      .baseColor = baseColor,
      .metallic = settings.metallic,
      .roughness = settings.roughness,
      .ambientOcclusion = settings.ambientOcclusion,
      .shadingModel = settings.pbrShadingEnabled ? MeshShadingModel::PhysicallyBased : MeshShadingModel::SimpleLit,
      .flatShadingEnabled = settings.flatShadingEnabled,
      .triangleEdgesEnabled = settings.triangleEdgesEnabled,
      .triangleEdgeColor = settings.triangleEdgeColor,
      .rimLightingEnabled = settings.rimLightingEnabled,
      .rimOpacityStrength = settings.rimOpacityStrength,
      .rimEmissionStrength = settings.rimEmissionStrength,
      .rimPower = settings.rimPower},
    glm::vec4{0.8f, 0.8f, 0.8f, 1.0f});
}

} // namespace rendering::mesh
