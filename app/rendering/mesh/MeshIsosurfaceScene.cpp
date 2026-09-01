#include "rendering/Rendering.h"

#include "common/UuidUtility.h"
#include "image/Image.h"
#include "image/Isosurface.h"
#include "logic/SurfaceUtility.h"
#include "logic/app/Data.h"
#include "rendering/PrivateMethods.h"
#include "rendering/mesh/MeshExtractionQueue.h"
#include "rendering/mesh/MeshExtractionJobs.h"
#include "rendering/mesh/MeshGeneration.h"
#include "rendering/mesh/MeshGpuSync.h"
#include "rendering/mesh/MeshImageAdapter.h"
#include "rendering/mesh/MeshImagePlaneRenderList.h"
#include "rendering/mesh/MeshIsosurfacePolicy.h"
#include "rendering/mesh/MeshRenderableFactory.h"
#include "rendering/mesh/MeshScene.h"
#include "windowing/View.h"

#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <format>
#include <iterator>
#include <memory>
#include <optional>
#include <ranges>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace
{

using MeshGeometryKey = rendering::mesh::MeshGeometryKey;
using MeshGeometryKeyHash = rendering::mesh::MeshGeometryKeyHash;
using MeshHandle = rendering::mesh::MeshHandle;
using MeshHandleMap = std::unordered_map<MeshGeometryKey, MeshHandle, MeshGeometryKeyHash>;

rendering::mesh::MeshHandle meshHandleForKey(const MeshGeometryKey& key, MeshHandleMap& handles)
{
  if (const auto existing = handles.find(key); existing != handles.end()) {
    return existing->second;
  }

  rendering::mesh::MeshHandle handle{
    .uid = generateRandomUuid(),
    .geometryVersion = rendering::mesh::MeshGeometryKeyHash{}(key)};
  handles.emplace(key, handle);
  return handle;
}

std::string isosurfaceMeshDescription(const Image& image, const Isosurface& surface, const std::size_t surfaceIndex)
{
  const std::string surfaceName = surface.name.empty() ? std::format("surface {}", surfaceIndex) : surface.name;
  return std::format("Isosurface mesh: {} - {}", image.settings().displayName(), surfaceName);
}

} // namespace

bool Rendering::renderIsosurfaceMeshesForView(
  const View& view,
  const CurrentImages& imageSegPairs,
  std::vector<rendering::mesh::MeshRenderable>* accumulatedRenderables)
{
  consumeCompletedMeshExtractions();

  if (imageSegPairs.empty()) {
    return false;
  }

  const std::vector<rendering::mesh::MeshClipPlane> clipPlanes = meshClipPlanes();
  std::vector<rendering::mesh::MeshRenderable> renderables;
  std::vector<rendering::mesh::MeshRenderable> imagePlaneBorderRenderables;
  std::vector<rendering::mesh::MeshImagePlaneRenderable> imagePlaneRenderables;
  bool allVisibleIsosurfacesHaveReadyMeshes = true;
  if (!accumulatedRenderables) {
    imagePlaneRenderables = collectMeshImagePlaneRenderablesForView(view, imagePlaneBorderRenderables);
  }

  for (const ImgSegPair& imgSegPair : imageSegPairs) {
    if (!imgSegPair.first) {
      continue;
    }

    const uuids::uuid& imageUid = *imgSegPair.first;
    const Image* image = m_appData.image(imageUid);
    if (!image) {
      continue;
    }

    const bool renderWarped = activeRenderableDeformationUid(imageUid).has_value();
    const ImageSettings& settings = image->settings();
    const uint32_t activeComponent = settings.activeComponent();
    const uint32_t activeTimePoint = image->timeAxis().clamp(settings.activeTimePoint());
    const auto isosurfaceUids = m_appData.isosurfaceUids(imageUid, activeComponent);
    renderables.reserve(renderables.size() + isosurfaceUids.size());
    std::shared_ptr<const Image> imageSnapshot;

    for (std::size_t surfaceIndex = 0; surfaceIndex < isosurfaceUids.size(); ++surfaceIndex) {
      const uuids::uuid& surfaceUid = isosurfaceUids[surfaceIndex];
      const Isosurface* surface = m_appData.isosurface(imageUid, activeComponent, surfaceUid);
      if (!surface) {
        spdlog::warn("Null isosurface {} for image {} when building mesh scene", surfaceUid, imageUid);
        continue;
      }

      const float effectiveOpacity = surface->opacity * settings.effectiveIsosurfaceOpacityModulator();
      const rendering::mesh::IsosurfaceMeshEligibility eligibility{
        .renderWarped = renderWarped,
        .valueEditInProgress = surface->valueEditInProgress,
        .opacity = effectiveOpacity,
        .visible = surface->visible};
      if (!rendering::mesh::canRenderIsosurfaceWithMesh(eligibility)) {
        if (eligibility.visible && eligibility.opacity > 0.0f) {
          allVisibleIsosurfacesHaveReadyMeshes = false;
        }
        continue;
      }

      const rendering::mesh::IsosurfaceMeshRequest request = rendering::mesh::makeScalarGridIsosurfaceRequest(
        imageUid,
        image->pixelDataRevision(),
        image->geometryRevision(),
        activeComponent,
        activeTimePoint,
        surface->value);
      const rendering::mesh::MeshGenerationOptions generationOptions{.threadCount = 0};
      const rendering::mesh::MeshGeometryKey key = rendering::mesh::geometryKeyForRequest(request);
      const rendering::mesh::MeshHandle handle = meshHandleForKey(key, m_meshHandles);

      if (!m_meshCpuCache.readyMesh(key)) {
        allVisibleIsosurfacesHaveReadyMeshes = false;
        const rendering::mesh::MeshCacheEntry* cacheEntry = m_meshCpuCache.find(key);
        const bool retry = m_meshCpuCache.canRetry(key);
        if ((!cacheEntry || retry) && m_meshExtractionQueue.canSubmit(key)) {
          if (!imageSnapshot) {
            imageSnapshot = std::make_shared<Image>(*image);
          }

          const std::string description = isosurfaceMeshDescription(*image, *surface, surfaceIndex);
          if (m_meshExtractionQueue.submit(
                key,
                description,
                rendering::mesh::makeIsosurfaceExtractionJob(request, generationOptions, imageSnapshot)))
          {
            m_meshCpuCache.markPending(key, retry ? cacheEntry->failureCount : 0);
          }
        }

        continue;
      }

      const rendering::mesh::MeshGpuSyncStatus syncStatus =
        rendering::mesh::syncReadyMeshToGpu(key, handle, m_meshCpuCache, m_meshGpuStore);
      if (
        syncStatus != rendering::mesh::MeshGpuSyncStatus::Uploaded &&
        syncStatus != rendering::mesh::MeshGpuSyncStatus::AlreadyCurrent)
      {
        allVisibleIsosurfacesHaveReadyMeshes = false;
        continue;
      }

      glm::vec4 color = getIsosurfaceColor(m_appData, *surface, settings, activeComponent, false);
      color.a = effectiveOpacity;
      const auto& globalMaterial = m_appData.renderData().m_meshSurfaceMaterialSettings;
      const rendering::mesh::IsosurfaceMeshStyle style{
        .material = rendering::mesh::meshMaterialForSurface(color, globalMaterial),
        .compositingMode = rendering::mesh::compositingModeForIsosurfaceAlpha(
          effectiveOpacity,
          globalMaterial.rimLightingEnabled,
          globalMaterial.rimOpacityStrength),
        .visible = surface->visible};
      rendering::mesh::MeshRenderable renderable =
        rendering::mesh::makeIsosurfaceRenderable(handle, image->transformations().worldDef_T_subject(), style);
      renderable.drawOptions.clipPlanes = clipPlanes;
      renderables.push_back(std::move(renderable));
    }
  }

  if (accumulatedRenderables) {
    accumulatedRenderables->insert(
      accumulatedRenderables->end(),
      std::make_move_iterator(renderables.begin()),
      std::make_move_iterator(renderables.end()));
    return allVisibleIsosurfacesHaveReadyMeshes;
  }

  renderables.insert(renderables.end(), imagePlaneBorderRenderables.begin(), imagePlaneBorderRenderables.end());

  rendering::mesh::MeshScene imagePlaneScene;
  imagePlaneScene.setImagePlaneRenderables(std::move(imagePlaneRenderables));
  const rendering::mesh::MeshImagePlaneRenderList imagePlaneList =
    rendering::mesh::buildImagePlaneRenderList(imagePlaneScene.imagePlaneRenderables());

  // Keep the crosshairs glyph in the same mesh pass as translucent surfaces so DDP can depth-order the glyph with the
  // surface instead of drawing the glyph later against an empty default depth buffer.
  appendMeshCrosshairsRenderableForView(view, renderables);

  if (renderables.empty() && imagePlaneList.imagePlanes.empty()) {
    return allVisibleIsosurfacesHaveReadyMeshes;
  }

  rendering::mesh::MeshScene scene;
  scene.setRenderables(renderables);
  const rendering::mesh::MeshRenderList list = rendering::mesh::buildRenderList(scene.renderables());
  if (rendering::mesh::visibleRenderableCount(list) != renderables.size()) {
    return false;
  }

  drawMeshRenderListForView(view, list, &imagePlaneList);
  return allVisibleIsosurfacesHaveReadyMeshes;
}

bool Rendering::renderCombinedSurfaceMeshesForView(const View& view)
{
  // The combined Seg + Iso mode is a mesh shader group, so it does not pass through the normal volume-rendering
  // branch. Preserve the interactive contract explicitly: draw segmentation meshes, image planes, and crosshairs,
  // then composite only the changing raycast surface over them with a transparent no-hit background.
  std::vector<rendering::mesh::MeshRenderable> renderables;
  const CurrentImages imageSegPairs = meshSceneImagesForView(view);
  const bool isosurfaceMeshesReady = renderIsosurfaceMeshesForView(view, imageSegPairs, &renderables);
  renderSegmentationMeshesForView(view, &renderables);

  std::vector<rendering::mesh::MeshRenderable> imagePlaneBorderRenderables;
  std::vector<rendering::mesh::MeshImagePlaneRenderable> imagePlaneRenderables =
    collectMeshImagePlaneRenderablesForView(view, imagePlaneBorderRenderables);
  renderables.insert(
    renderables.end(),
    std::make_move_iterator(imagePlaneBorderRenderables.begin()),
    std::make_move_iterator(imagePlaneBorderRenderables.end()));
  appendMeshCrosshairsRenderableForView(view, renderables);

  rendering::mesh::MeshScene imagePlaneScene;
  imagePlaneScene.setImagePlaneRenderables(std::move(imagePlaneRenderables));
  const rendering::mesh::MeshImagePlaneRenderList imagePlaneList =
    rendering::mesh::buildImagePlaneRenderList(imagePlaneScene.imagePlaneRenderables());

  const bool hasMeshSceneContent = !renderables.empty() || !imagePlaneList.imagePlanes.empty();
  if (hasMeshSceneContent) {
    rendering::mesh::MeshScene scene;
    scene.setRenderables(std::move(renderables));
    const rendering::mesh::MeshRenderList list = rendering::mesh::buildRenderList(scene.renderables());
    drawMeshRenderListForView(view, list, &imagePlaneList);
  }
  if (!isosurfaceMeshesReady) {
    renderVolumeImagesForView(view, true);
  }
  else {
    m_isosurfaceRaycastHandoff.reset();
  }
  return hasMeshSceneContent || !isosurfaceMeshesReady;
}
