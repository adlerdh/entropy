#include "rendering/Rendering.h"

#include "logic/app/Data.h"
#include "logic/app/ParcellationLabelTable.h"
#include "image/Image.h"
#include "image/Isosurface.h"
#include "rendering/PrivateMethods.h"
#include "rendering/RenderData.h"
#include "rendering/mesh/AmbientOcclusionPass.h"
#include "rendering/mesh/AmbientOcclusionResources.h"
#include "rendering/mesh/MeshBounds.h"
#include "rendering/mesh/MeshDdpPass.h"
#include "rendering/mesh/MeshDdpPolicy.h"
#include "rendering/mesh/MeshExtractionQueue.h"
#include "rendering/mesh/MeshExtraction.h"
#include "rendering/mesh/MeshIsosurfacePolicy.h"
#include "rendering/mesh/MeshResourceLifecycle.h"
#include "rendering/mesh/MeshSegmentationPolicy.h"
#include "rendering/mesh/MeshShadowMapPass.h"
#include "rendering/mesh/MeshShadowMapProjection.h"
#include "rendering/mesh/MeshShadowMapResources.h"
#include "rendering/mesh/MeshViewContext.h"
#include "rendering/mesh/MeshViewViewport.h"
#include "windowing/View.h"

#include <glad/glad.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <array>
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

bool hasImagePlaneDdpRenderables(const rendering::mesh::MeshImagePlaneRenderList* const imagePlaneList) noexcept
{
  return imagePlaneList && !imagePlaneList->imagePlanes.empty();
}

} // namespace

void Rendering::reconcileExtractedMeshResources()
{
  rendering::mesh::MeshGeometryKeySet liveKeys;

  std::erase_if(m_segmentationLabelInventories, [this](const auto& entry) { return !m_appData.seg(entry.first); });

  for (const uuids::uuid& imageUid : m_appData.imageUidsOrdered()) {
    const Image* image = m_appData.image(imageUid);
    if (!image) {
      continue;
    }
    const uint32_t component = image->settings().activeComponent();
    const uint32_t timePoint = image->timeAxis().clamp(image->settings().activeTimePoint());
    for (const uuids::uuid& surfaceUid : m_appData.isosurfaceUids(imageUid, component)) {
      const Isosurface* surface = m_appData.isosurface(imageUid, component, surfaceUid);
      if (!surface) {
        continue;
      }
      liveKeys.insert(rendering::mesh::geometryKeyForRequest(rendering::mesh::makeScalarGridIsosurfaceRequest(
        imageUid,
        image->pixelDataRevision(),
        image->geometryRevision(),
        component,
        timePoint,
        surface->value)));
    }
  }

  for (const uuids::uuid& segmentationUid : m_appData.segUidsOrdered()) {
    const Image* segmentation = m_appData.seg(segmentationUid);
    if (!segmentation) {
      continue;
    }
    const auto tableUid = m_appData.labelTableUid(segmentation->settings().labelTableIndex());
    const ParcellationLabelTable* labelTable = tableUid ? m_appData.labelTable(*tableUid) : nullptr;
    if (!labelTable) {
      continue;
    }
    const uint32_t timePoint = segmentation->timeAxis().clamp(segmentation->settings().activeTimePoint());
    const auto inventory = m_segmentationLabelInventories.find(segmentationUid);
    const bool inventoryIsCurrent = inventory != m_segmentationLabelInventories.end() &&
                                    inventory->second.pixelDataRevision == segmentation->pixelDataRevision() &&
                                    inventory->second.timePoint == timePoint;
    for (std::size_t labelIndex = 1; labelIndex < labelTable->numLabels(); ++labelIndex) {
      if (inventoryIsCurrent && !inventory->second.presentLabelValues.contains(static_cast<int64_t>(labelIndex))) {
        continue;
      }
      liveKeys.insert(rendering::mesh::geometryKeyForRequest(rendering::mesh::makeScalarGridSegmentationRequest(
        segmentationUid,
        segmentation->pixelDataRevision(),
        segmentation->geometryRevision(),
        static_cast<int64_t>(labelIndex),
        timePoint)));
    }
  }

  rendering::mesh::reconcileExtractedMeshResources(
    liveKeys,
    m_meshCpuCache,
    m_meshHandles,
    [this](const uuids::uuid& handleUid) { m_meshGpuStore.remove(handleUid); });
}

void Rendering::consumeCompletedMeshExtractions()
{
  for (rendering::mesh::MeshExtractionJobResult& result : m_meshExtractionQueue.takeCompleted()) {
    rendering::mesh::applyExtractionJobResult(std::move(result), m_meshCpuCache);
  }
}

void Rendering::updateMeshExtractionStatus()
{
  if (!m_appData.guiData().m_meshExtractionStatus) {
    return;
  }

  const std::size_t activeJobs = m_meshExtractionQueue.activeCount();
  std::vector<std::string> activeDescriptions = m_meshExtractionQueue.activeDescriptions();
  std::scoped_lock lock(m_appData.guiData().m_meshExtractionStatus->mutex);
  m_appData.guiData().m_meshExtractionStatus->visible = activeJobs > 0;
  m_appData.guiData().m_meshExtractionStatus->title = activeJobs == 1 ? "Computing mesh" : "Computing meshes";
  m_appData.guiData().m_meshExtractionStatus->activeJobs = activeJobs;
  m_appData.guiData().m_meshExtractionStatus->descriptions = std::move(activeDescriptions);
}

void Rendering::clearMeshViewBackgroundForView(const View& view)
{
  const rendering::mesh::ScopedMeshViewViewport scopedViewport{view, m_appData.windowData()};

  std::array<GLfloat, 4> previousClearColor{};
  glGetFloatv(GL_COLOR_CLEAR_VALUE, previousClearColor.data());

  const auto& bg = m_appData.renderData().m_3dBackgroundColor;
  glClearColor(bg.r, bg.g, bg.b, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
  glClearColor(previousClearColor[0], previousClearColor[1], previousClearColor[2], previousClearColor[3]);
}

void Rendering::drawMeshRenderListForView(
  const View& view,
  const rendering::mesh::MeshRenderList& list,
  const rendering::mesh::MeshImagePlaneRenderList* const imagePlaneList)
{
  const rendering::mesh::ScopedMeshViewViewport scopedViewport{view, m_appData.windowData()};
  std::array<GLint, 4> viewViewport{};
  glGetIntegerv(GL_VIEWPORT, viewViewport.data());

  const RenderData& renderData = m_appData.renderData();
  rendering::mesh::MeshDrawContext context = rendering::mesh::meshDrawContextForView(
    m_meshGpuStore,
    view,
    glm::vec4{
      renderData.m_lightingAmbient,
      renderData.m_lightingDiffuse,
      renderData.m_lightingSpecular,
      renderData.m_lightingSpecularPower});
  context.viewportOrigin = glm::ivec2{viewViewport[0], viewViewport[1]};
  context.advancedLighting = rendering::mesh::meshAdvancedLightingPlan(
    renderData.m_meshAdvancedLightingSettings,
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
      rendering::mesh::shadowCastingRenderables(list);

    rendering::mesh::MeshDrawContext shadowContext = context;
    if (projection) {
      shadowContext.clip_T_world = projection->lightClip_T_world;
      shadowContext.lightDirectionWorld = projection->lightDirectionWorld;
      shadowContext.shadowDepthPass = true;
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
    const auto& aoRenderables = list.opaque;
    rendering::mesh::MeshDrawContext aoContext = context;
    aoContext.advancedLighting.shadows.state = rendering::mesh::MeshAdvancedLightingFeatureState::Disabled;
    aoContext.shadowDepthTexture = nullptr;
    aoContext.advancedLighting.ambientOcclusion.state = rendering::mesh::MeshAdvancedLightingFeatureState::Disabled;
    aoContext.ambientOcclusionTexture = nullptr;
    if (rendering::mesh::renderMeshAmbientOcclusion(rendering::mesh::MeshAmbientOcclusionRenderRequest{
          .resources = m_meshAmbientOcclusionResources,
          .renderables = aoRenderables,
          .context = aoContext,
          .plan = context.advancedLighting.ambientOcclusion,
          .meshRenderer = m_meshRenderer,
          .geometryProgram = m_meshAmbientOcclusionGeometryProgram,
          .resolveProgram = m_meshAmbientOcclusionResolveProgram,
          .filterProgram = m_meshAmbientOcclusionFilterProgram}))
    {
      context.ambientOcclusionTexture = &m_meshAmbientOcclusionResources.occlusionTexture();
    }
  }

  const rendering::mesh::MeshDdpSettings& ddpSettings = renderData.m_meshDdpSettings;
  rendering::mesh::MeshDdpPlan ddpPlan = rendering::mesh::meshDdpPlanForRenderList(list, ddpSettings);
  if (hasImagePlaneDdpRenderables(imagePlaneList)) {
    ddpPlan = rendering::mesh::meshDdpPlanWithExtraRenderables(
      ddpPlan,
      ddpSettings,
      static_cast<uint32_t>(imagePlaneList->imagePlanes.size()));
  }
  for (const std::string& diagnostic : rendering::mesh::meshDdpDiagnostics(ddpPlan, ddpSettings)) {
    spdlog::debug("{}", diagnostic);
  }
  if (ddpPlan.active) {
    const std::vector<std::reference_wrapper<const rendering::mesh::MeshRenderable>> ddpRenderables =
      ddpSurfaceRenderables(list);
    // DDP peels into view-sized textures with a (0, 0) viewport. AO is also view-sized, so mesh peel shaders must
    // address it in that local coordinate system rather than subtracting the window-space viewport origin.
    rendering::mesh::MeshDrawContext ddpContext = context;
    ddpContext.viewportOrigin = glm::ivec2{0};
    rendering::mesh::renderMeshDdpAlphaOver(rendering::mesh::MeshDdpRenderRequest{
      .resources = m_meshDdpResources,
      .renderables = ddpRenderables,
      .context = ddpContext,
      .plan = ddpPlan,
      .meshRenderer = m_meshRenderer,
      .initProgram = m_meshDdpInitProgram,
      .peelProgram = m_meshDdpPeelProgram,
      .backBlendProgram = m_meshDdpBackBlendProgram,
      .resolveProgram = m_meshDdpResolveProgram,
      .drawExtraDepthBounds = imagePlaneList
                                 ? [this, &view, imagePlaneList, &context]() {
                                     drawMeshImagePlaneDdpDepthBoundsForView(view, *imagePlaneList, context);
                                   }
                                 : std::function<void()>{},
      .drawExtraPeelLayers = imagePlaneList
                               ? [this, &view, imagePlaneList, &context](GLTexture& previousDepthBounds,
                                                                           GLTexture& previousFrontColor) {
                                   drawMeshImagePlaneDdpPeelLayersForView(
                                     view,
                                     *imagePlaneList,
                                     context,
                                     previousDepthBounds,
                                     previousFrontColor);
                                 }
                               : std::function<void(GLTexture&, GLTexture&)>{}});
  }
  else {
    m_meshRenderer.drawOpaque(list, context, m_meshProgram);
  }

  m_meshRenderer.drawAdditive(list, context, m_meshProgram);
  m_meshRenderer.drawMultiplicative(list, context, m_meshProgram);
  setupOpenGLState();
}
