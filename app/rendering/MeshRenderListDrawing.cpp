#include "rendering/Rendering.h"

#include "logic/app/Data.h"
#include "rendering/PrivateMethods.h"
#include "rendering/RenderData.h"
#include "rendering/mesh/MeshAmbientOcclusionPass.h"
#include "rendering/mesh/MeshAmbientOcclusionResources.h"
#include "rendering/mesh/MeshBounds.h"
#include "rendering/mesh/MeshDdpPass.h"
#include "rendering/mesh/MeshDdpPolicy.h"
#include "rendering/mesh/MeshExtractionQueue.h"
#include "rendering/mesh/MeshShadowMapPass.h"
#include "rendering/mesh/MeshShadowMapProjection.h"
#include "rendering/mesh/MeshShadowMapResources.h"
#include "rendering/mesh/MeshViewContext.h"
#include "rendering/mesh/MeshViewViewport.h"
#include "windowing/View.h"

#include <spdlog/spdlog.h>

#include <functional>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace
{

std::vector<std::reference_wrapper<const rendering::mesh::MeshRenderable>> ddpSurfaceRenderables(
  const rendering::mesh::MeshRenderList& list)
{
  std::vector<std::reference_wrapper<const rendering::mesh::MeshRenderable>> renderables;
  renderables.reserve(list.opaque.size() + list.alphaOverDdp.size());
  renderables.insert(renderables.end(), list.opaque.begin(), list.opaque.end());
  renderables.insert(renderables.end(), list.alphaOverDdp.begin(), list.alphaOverDdp.end());
  return renderables;
}

std::vector<std::reference_wrapper<const rendering::mesh::MeshRenderable>> shadowCastingRenderables(
  const rendering::mesh::MeshRenderList& list)
{
  std::vector<std::reference_wrapper<const rendering::mesh::MeshRenderable>> renderables;
  renderables.reserve(list.opaque.size() + list.alphaOverDdp.size());
  renderables.insert(renderables.end(), list.opaque.begin(), list.opaque.end());
  renderables.insert(renderables.end(), list.alphaOverDdp.begin(), list.alphaOverDdp.end());
  return renderables;
}

} // namespace

void Rendering::consumeCompletedMeshExtractions()
{
  for (rendering::mesh::MeshExtractionJobResult& result : m_meshExtractionQueue.takeCompleted()) {
    rendering::mesh::applyExtractionJobResult(std::move(result), m_meshCpuCache);
  }
}

void Rendering::drawMeshRenderListForView(const View& view, const rendering::mesh::MeshRenderList& list)
{
  const rendering::mesh::ScopedMeshViewViewport scopedViewport{view, m_appData.windowData()};
  rendering::mesh::MeshDrawContext context = rendering::mesh::meshDrawContextForView(m_meshGpuStore, view);
  context.advancedLighting = rendering::mesh::meshAdvancedLightingPlan(
    m_appData.renderData().m_meshAdvancedLightingSettings,
    rendering::mesh::MeshAdvancedLightingCapabilities{
      .shadowMapPassAvailable = true,
      .ambientOcclusionPassAvailable = true});

  const auto cpuMeshLookup = [this](const rendering::mesh::MeshHandle& handle) -> const rendering::mesh::MeshData* {
    for (const auto& [key, storedHandle] : m_meshHandles) {
      if (storedHandle == handle) {
        return m_meshCpuCache.readyMesh(key);
      }
    }
    return nullptr;
  };

  const std::optional<rendering::mesh::MeshBounds> sceneBounds =
    rendering::mesh::computeWorldBounds(list, cpuMeshLookup);
  if (
    context.advancedLighting.shadows.state == rendering::mesh::MeshAdvancedLightingFeatureState::Enabled && sceneBounds)
  {
    const std::optional<rendering::mesh::MeshShadowMapProjection> projection =
      rendering::mesh::meshShadowMapProjectionForBounds(*sceneBounds, context.lightDirectionWorld);
    const std::vector<std::reference_wrapper<const rendering::mesh::MeshRenderable>> shadowRenderables =
      shadowCastingRenderables(list);

    rendering::mesh::MeshDrawContext shadowContext = context;
    if (projection) {
      shadowContext.clip_T_world = projection->lightClip_T_world;
      shadowContext.advancedLighting.shadows.state = rendering::mesh::MeshAdvancedLightingFeatureState::Disabled;
      shadowContext.shadowDepthTexture = nullptr;
    }

    if (
      projection && rendering::mesh::renderMeshShadowMap(rendering::mesh::MeshShadowMapRenderRequest{
                      .resources = m_meshShadowMapResources,
                      .renderables = shadowRenderables,
                      .context = shadowContext,
                      .plan = context.advancedLighting.shadows,
                      .meshRenderer = m_meshRenderer,
                      .depthProgram = m_meshShadowDepthProgram}))
    {
      context.shadowLightClip_T_world = projection->lightClip_T_world;
      context.lightDirectionWorld = projection->lightDirectionWorld;
      context.shadowDepthTexture = &m_meshShadowMapResources.depthTexture();
    }
  }

  if (context.advancedLighting.ambientOcclusion.state == rendering::mesh::MeshAdvancedLightingFeatureState::Enabled) {
    const std::vector<std::reference_wrapper<const rendering::mesh::MeshRenderable>> aoRenderables =
      shadowCastingRenderables(list);
    rendering::mesh::MeshDrawContext aoContext = context;
    aoContext.advancedLighting.ambientOcclusion.state = rendering::mesh::MeshAdvancedLightingFeatureState::Disabled;
    aoContext.ambientOcclusionTexture = nullptr;
    if (rendering::mesh::renderMeshAmbientOcclusion(rendering::mesh::MeshAmbientOcclusionRenderRequest{
          .resources = m_meshAmbientOcclusionResources,
          .renderables = aoRenderables,
          .context = aoContext,
          .plan = context.advancedLighting.ambientOcclusion,
          .meshRenderer = m_meshRenderer,
          .geometryProgram = m_meshAmbientOcclusionGeometryProgram,
          .resolveProgram = m_meshAmbientOcclusionResolveProgram}))
    {
      context.ambientOcclusionTexture = &m_meshAmbientOcclusionResources.occlusionTexture();
    }
  }

  const rendering::mesh::MeshDdpPlan ddpPlan = rendering::mesh::meshDdpPlanForRenderList(list, {});
  for (const std::string& diagnostic : rendering::mesh::meshDdpDiagnostics(ddpPlan, {})) {
    spdlog::debug("{}", diagnostic);
  }
  if (ddpPlan.active) {
    const std::vector<std::reference_wrapper<const rendering::mesh::MeshRenderable>> ddpRenderables =
      ddpSurfaceRenderables(list);
    rendering::mesh::renderMeshDdpAlphaOver(rendering::mesh::MeshDdpRenderRequest{
      .resources = m_meshDdpResources,
      .renderables = ddpRenderables,
      .context = context,
      .plan = ddpPlan,
      .meshRenderer = m_meshRenderer,
      .initProgram = m_meshDdpInitProgram,
      .peelProgram = m_meshDdpPeelProgram,
      .backBlendProgram = m_meshDdpBackBlendProgram,
      .resolveProgram = m_meshDdpResolveProgram});
  }
  else {
    m_meshRenderer.drawOpaque(list, context, m_meshProgram);
  }

  m_meshRenderer.drawAdditive(list, context, m_meshProgram);
  m_meshRenderer.drawMultiplicative(list, context, m_meshProgram);
  setupOpenGLState();
}
