#include "rendering/mesh/MeshAdvancedLighting.h"

#include <algorithm>
#include <cmath>

namespace rendering::mesh
{

namespace
{

constexpr uint32_t k_minShadowMapSizePixels = 128;
constexpr uint32_t k_maxShadowMapSizePixels = 8192;
constexpr uint32_t k_minAmbientOcclusionSamples = 1;
constexpr uint32_t k_maxAmbientOcclusionSamples = 128;
constexpr float k_minDepthBias = 0.0f;
constexpr float k_maxDepthBias = 0.1f;
constexpr float k_minAmbientOcclusionRadiusPixels = 1.0f;
constexpr float k_maxAmbientOcclusionRadiusPixels = 128.0f;

float finiteOr(const float value, const float fallback) noexcept
{
  return std::isfinite(value) ? value : fallback;
}

MeshAdvancedLightingFeatureState featureState(const bool requested, const bool available) noexcept
{
  if (!requested) {
    return MeshAdvancedLightingFeatureState::Disabled;
  }

  return available ? MeshAdvancedLightingFeatureState::Enabled : MeshAdvancedLightingFeatureState::Unavailable;
}

} // namespace

bool isRequestedButUnavailable(const MeshAdvancedLightingFeatureState state) noexcept
{
  return state == MeshAdvancedLightingFeatureState::Unavailable;
}

MeshShadowPlan meshShadowPlan(
  const MeshShadowSettings& settings,
  const MeshAdvancedLightingCapabilities& capabilities) noexcept
{
  const MeshShadowSettings defaults;
  return MeshShadowPlan{
    .state = featureState(settings.enabled, capabilities.shadowMapPassAvailable),
    .mapSizePixels = std::clamp(settings.mapSizePixels, k_minShadowMapSizePixels, k_maxShadowMapSizePixels),
    .strength = std::clamp(finiteOr(settings.strength, defaults.strength), 0.0f, 1.0f),
    .depthBias = std::clamp(finiteOr(settings.depthBias, defaults.depthBias), k_minDepthBias, k_maxDepthBias)};
}

MeshAmbientOcclusionPlan meshAmbientOcclusionPlan(
  const MeshAmbientOcclusionSettings& settings,
  const MeshAdvancedLightingCapabilities& capabilities) noexcept
{
  const MeshAmbientOcclusionSettings defaults;
  return MeshAmbientOcclusionPlan{
    .state = featureState(settings.enabled, capabilities.ambientOcclusionPassAvailable),
    .radiusPixels = std::clamp(
      finiteOr(settings.radiusPixels, defaults.radiusPixels),
      k_minAmbientOcclusionRadiusPixels,
      k_maxAmbientOcclusionRadiusPixels),
    .strength = std::clamp(finiteOr(settings.strength, defaults.strength), 0.0f, 1.0f),
    .sampleCount = std::clamp(settings.sampleCount, k_minAmbientOcclusionSamples, k_maxAmbientOcclusionSamples)};
}

MeshAdvancedLightingPlan meshAdvancedLightingPlan(
  const MeshAdvancedLightingSettings& settings,
  const MeshAdvancedLightingCapabilities& capabilities) noexcept
{
  return MeshAdvancedLightingPlan{
    .shadows = meshShadowPlan(settings.shadows, capabilities),
    .ambientOcclusion = meshAmbientOcclusionPlan(settings.ambientOcclusion, capabilities)};
}

std::vector<std::string> advancedLightingDiagnostics(const MeshAdvancedLightingPlan& plan)
{
  std::vector<std::string> diagnostics;
  if (isRequestedButUnavailable(plan.shadows.state)) {
    diagnostics.emplace_back("Mesh shadow maps were requested, but the shadow-map render pass is not available");
  }
  if (isRequestedButUnavailable(plan.ambientOcclusion.state)) {
    diagnostics.emplace_back(
      "Mesh ambient occlusion was requested, but the ambient occlusion render pass is not available");
  }
  return diagnostics;
}

} // namespace rendering::mesh
