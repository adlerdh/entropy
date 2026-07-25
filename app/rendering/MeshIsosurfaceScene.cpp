#include "rendering/Rendering.h"

#include "common/UuidUtility.h"
#include "image/Image.h"
#include "image/Isosurface.h"
#include "logic/SurfaceUtility.h"
#include "logic/app/Data.h"
#include "rendering/PrivateMethods.h"
#include "rendering/mesh/MeshExtractionQueue.h"
#include "rendering/mesh/MeshGpuSync.h"
#include "rendering/mesh/MeshImageAdapter.h"
#include "rendering/mesh/MeshIsosurfacePolicy.h"
#include "rendering/mesh/MeshRenderableFactory.h"
#include "rendering/mesh/MeshScene.h"
#include "windowing/View.h"

#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>
#include <spdlog/spdlog.h>

#include <optional>
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

rendering::mesh::MeshMaterial meshMaterialForIsosurface(const Isosurface& surface, const glm::vec4& color)
{
  rendering::mesh::MeshMaterial material;
  material.baseColor = color;
  material.metallic = surface.material.metallic;
  material.roughness = surface.material.roughness;
  material.ambientOcclusion = surface.material.ambientOcclusion;
  material.shadingModel = surface.material.usePbrShading ? rendering::mesh::MeshShadingModel::PhysicallyBased
                                                         : rendering::mesh::MeshShadingModel::SimpleLit;
  return material;
}

} // namespace

bool Rendering::renderIsosurfaceMeshesForView(const View& view, const ImgSegPair& imgSegPair, const bool renderWarped)
{
  consumeCompletedMeshExtractions();

  if (!imgSegPair.first) {
    return false;
  }

  const uuids::uuid& imageUid = *imgSegPair.first;
  const Image* image = m_appData.image(imageUid);
  if (!image) {
    return false;
  }

  const ImageSettings& settings = image->settings();
  const uint32_t activeComponent = settings.activeComponent();
  const uint32_t activeTimePoint = image->timeAxis().clamp(settings.activeTimePoint());
  const auto isosurfaceUids = m_appData.isosurfaceUids(imageUid, activeComponent);
  const std::vector<rendering::mesh::MeshClipPlane> clipPlanes = meshClipPlanes();
  std::vector<rendering::mesh::MeshRenderable> renderables;
  renderables.reserve(isosurfaceUids.size());

  for (const uuids::uuid& surfaceUid : isosurfaceUids) {
    const Isosurface* surface = m_appData.isosurface(imageUid, activeComponent, surfaceUid);
    if (!surface) {
      spdlog::warn("Null isosurface {} for image {} when building mesh scene", surfaceUid, imageUid);
      return false;
    }

    const float effectiveOpacity = surface->opacity * settings.isosurfaceOpacityModulator();
    const rendering::mesh::IsosurfaceMeshEligibility eligibility{
      .renderWarped = renderWarped,
      .rimLightingEnabled = surface->rimLightingEnabled,
      .valueEditInProgress = surface->valueEditInProgress,
      .opacity = effectiveOpacity,
      .visible = surface->visible};
    if (!rendering::mesh::canRenderIsosurfaceWithMesh(eligibility)) {
      return false;
    }

    const rendering::mesh::IsosurfaceMeshRequest request = rendering::mesh::makeScalarGridIsosurfaceRequest(
      imageUid,
      image->pixelDataRevision(),
      image->geometryRevision(),
      activeComponent,
      activeTimePoint,
      surface->value);
    const rendering::mesh::MeshGeometryKey key = rendering::mesh::geometryKeyForRequest(request);
    const rendering::mesh::MeshHandle handle = meshHandleForKey(key, m_meshHandles);

    if (!m_meshCpuCache.readyMesh(key)) {
      if (!m_meshCpuCache.contains(key) && !m_meshExtractionQueue.active(key)) {
        std::optional<rendering::mesh::ScalarGrid3D> grid = rendering::mesh::scalarGridFromImageComponent(
          *image,
          request.component,
          request.timePoint,
          rendering::mesh::MeshCoordinateSpace::World);
        if (!grid) {
          return false;
        }

        m_meshCpuCache.markPending(key);
        m_meshExtractionQueue.submit(key, [request, key, grid = std::move(*grid)]() mutable {
          std::optional<rendering::mesh::MeshData> mesh =
            rendering::mesh::extractIsosurfaceMesh(grid, request.isoValue);
          if (!mesh) {
            return rendering::mesh::MeshExtractionJobResult{
              .key = key,
              .result = std::nullopt,
              .diagnostics = {std::string{"No isosurface mesh could be extracted"}}};
          }

          return rendering::mesh::MeshExtractionJobResult{
            .key = key,
            .result = rendering::mesh::MeshExtractionResult{.key = key, .mesh = std::move(*mesh)}};
        });
      }

      return false;
    }

    const rendering::mesh::MeshGpuSyncStatus syncStatus =
      rendering::mesh::syncReadyMeshToGpu(key, handle, m_meshCpuCache, m_meshGpuStore);
    if (
      syncStatus != rendering::mesh::MeshGpuSyncStatus::Uploaded &&
      syncStatus != rendering::mesh::MeshGpuSyncStatus::AlreadyCurrent)
    {
      return false;
    }

    glm::vec4 color = getIsosurfaceColor(m_appData, *surface, settings, activeComponent, false);
    color.a = effectiveOpacity;
    const rendering::mesh::IsosurfaceMeshStyle style{
      .material = meshMaterialForIsosurface(*surface, color),
      .visible = surface->visible};
    rendering::mesh::MeshRenderable renderable =
      rendering::mesh::makeIsosurfaceRenderable(handle, glm::mat4{1.0f}, style);
    renderable.drawOptions.clipPlanes = clipPlanes;
    renderables.push_back(std::move(renderable));
  }

  if (renderables.empty()) {
    return false;
  }

  rendering::mesh::MeshScene scene;
  scene.setRenderables(renderables);
  const rendering::mesh::MeshRenderList list = rendering::mesh::buildRenderList(scene.renderables());
  if (rendering::mesh::visibleRenderableCount(list) != renderables.size()) {
    return false;
  }

  drawMeshRenderListForView(view, list);
  return true;
}
