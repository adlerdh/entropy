#include "rendering/Rendering.h"

#include "image/Image.h"
#include "image/Isosurface.h"
#include "logic/SurfaceUtility.h"
#include "logic/app/Data.h"
#include "logic/app/ParcellationLabelTable.h"
#include "logic/camera/CameraHelpers.h"
#include "rendering/PrivateMethods.h"
#include "rendering/mesh/MeshIsosurfacePolicy.h"
#include "rendering/mesh/MeshPicking.h"
#include "rendering/mesh/MeshRenderableFactory.h"
#include "rendering/mesh/MeshRenderList.h"
#include "rendering/mesh/MeshSegmentationPolicy.h"
#include "windowing/View.h"

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include <optional>
#include <unordered_map>
#include <utility>
#include <vector>

namespace
{

using MeshHandleMap = std::
  unordered_map<rendering::mesh::MeshGeometryKey, rendering::mesh::MeshHandle, rendering::mesh::MeshGeometryKeyHash>;

const rendering::mesh::MeshHandle* findMeshHandle(
  const rendering::mesh::MeshGeometryKey& key,
  const MeshHandleMap& handles)
{
  const auto it = handles.find(key);
  return it == handles.end() ? nullptr : &it->second;
}

const rendering::mesh::MeshData* meshDataForHandle(
  const rendering::mesh::MeshHandle& handle,
  const MeshHandleMap& handles,
  const rendering::mesh::MeshCache& cache)
{
  for (const auto& [key, candidateHandle] : handles) {
    if (candidateHandle == handle) {
      return cache.readyMesh(key);
    }
  }

  return nullptr;
}

glm::vec4 normalizedLabelColor(const ParcellationLabelTable& labelTable, const std::size_t labelIndex)
{
  return glm::vec4{glm::vec3{labelTable.getColor(labelIndex)}, static_cast<float>(labelTable.getAlpha(labelIndex))} /
         255.0f;
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
  material.rimLightingEnabled = surface.rimLightingEnabled;
  material.rimOpacityStrength = surface.rimOpacityStrength;
  material.rimEmissionStrength = surface.rimEmissionStrength;
  material.rimPower = surface.rimPower;
  return material;
}

} // namespace

std::optional<glm::vec3> Rendering::pickNearestMeshWorldPositionForView(const View& view, const glm::vec2& viewClipPos)
{
  if (ViewType::ThreeD != view.viewType()) {
    return std::nullopt;
  }
  if (!m_appData.renderData().m_meshPickingEnabled) {
    return std::nullopt;
  }

  const CurrentImages imageSegPairs = meshSceneImagesForView(view);
  if (imageSegPairs.empty()) {
    return std::nullopt;
  }

  const std::vector<rendering::mesh::MeshClipPlane> clipPlanes = meshClipPlanes();
  std::vector<rendering::mesh::MeshRenderable> renderables;
  if (rendersIsosurfaces(view.renderMode())) {
    for (const ImgSegPair& imageSegPair : imageSegPairs) {
      if (!imageSegPair.first) {
        continue;
      }
      const uuids::uuid& imageUid = *imageSegPair.first;
      const Image* image = m_appData.image(imageUid);
      if (!image) {
        continue;
      }

      const ImageSettings& settings = image->settings();
      if (!settings.isosurfacesVisible() || !settings.showIsosurfacesIn3D() || activeRenderableDeformationUid(imageUid))
      {
        continue;
      }

      const uint32_t activeComponent = settings.activeComponent();
      const uint32_t activeTimePoint = image->timeAxis().clamp(settings.activeTimePoint());
      for (const uuids::uuid& surfaceUid : m_appData.isosurfaceUids(imageUid, activeComponent)) {
        const Isosurface* surface = m_appData.isosurface(imageUid, activeComponent, surfaceUid);
        if (!surface) {
          continue;
        }

        const float effectiveOpacity = surface->opacity * settings.isosurfaceOpacityModulator();
        if (!rendering::mesh::canRenderIsosurfaceWithMesh(
              {.renderWarped = false,
               .valueEditInProgress = surface->valueEditInProgress,
               .opacity = effectiveOpacity,
               .visible = surface->visible && surface->showIn3d}))
        {
          continue;
        }

        const rendering::mesh::IsosurfaceMeshRequest request = rendering::mesh::makeScalarGridIsosurfaceRequest(
          imageUid,
          image->pixelDataRevision(),
          image->geometryRevision(),
          activeComponent,
          activeTimePoint,
          surface->value);
        const rendering::mesh::MeshGeometryKey key = rendering::mesh::geometryKeyForRequest(request);
        const rendering::mesh::MeshHandle* handle = findMeshHandle(key, m_meshHandles);
        if (!handle || !m_meshCpuCache.readyMesh(key)) {
          continue;
        }

        glm::vec4 color = getIsosurfaceColor(m_appData, *surface, settings, activeComponent, false);
        color.a = effectiveOpacity;
        rendering::mesh::MeshRenderable renderable = rendering::mesh::makeIsosurfaceRenderable(
          *handle,
          glm::mat4{1.0f},
          rendering::mesh::IsosurfaceMeshStyle{
            .material = meshMaterialForIsosurface(*surface, color),
            .compositingMode = rendering::mesh::compositingModeForIsosurfaceAlpha(
              effectiveOpacity,
              surface->rimLightingEnabled,
              surface->rimOpacityStrength),
            .visible = surface->visible});
        renderable.drawOptions.clipPlanes = clipPlanes;
        renderables.push_back(std::move(renderable));
      }
    }
  }
  if (rendersSegmentations(view.renderMode())) {
    for (const ImgSegPair& imageSegPair : imageSegPairs) {
      if (!imageSegPair.second) {
        continue;
      }

      const uuids::uuid& segUid = *imageSegPair.second;
      const Image* seg = m_appData.seg(segUid);
      if (!seg || !seg->settings().visibility()) {
        continue;
      }

      const auto tableUid = m_appData.labelTableUid(seg->settings().labelTableIndex());
      const ParcellationLabelTable* labelTable = tableUid ? m_appData.labelTable(*tableUid) : nullptr;
      if (!labelTable) {
        continue;
      }

      const uint32_t timePoint = seg->timeAxis().clamp(seg->settings().activeTimePoint());
      const rendering::mesh::SegmentationLabelSet* presentLabels = presentSegmentationLabels(segUid, *seg, timePoint);
      if (!presentLabels) {
        continue;
      }

      const float segmentationOpacity = static_cast<float>(seg->settings().opacity());
      for (std::size_t labelIndex = 1; labelIndex < labelTable->numLabels(); ++labelIndex) {
        const rendering::mesh::SegmentationLabelMeshState labelState{
          .showMesh = labelTable->getShowMesh(labelIndex),
          .opacity = segmentationOpacity};
        if (!rendering::mesh::shouldRenderSegmentationLabelMesh(labelState)) {
          continue;
        }

        const int64_t labelValue = static_cast<int64_t>(labelIndex);
        if (!presentLabels->contains(labelValue)) {
          continue;
        }
        const rendering::mesh::SegmentationMeshRequest request = rendering::mesh::makeScalarGridSegmentationRequest(
          segUid,
          seg->pixelDataRevision(),
          seg->geometryRevision(),
          labelValue,
          timePoint);
        const rendering::mesh::MeshGeometryKey key = rendering::mesh::geometryKeyForRequest(request);
        const rendering::mesh::MeshHandle* handle = findMeshHandle(key, m_meshHandles);
        if (!handle || !m_meshCpuCache.readyMesh(key)) {
          continue;
        }

        rendering::mesh::MeshRenderable renderable = rendering::mesh::makeSegmentationLabelRenderable(
          *handle,
          glm::mat4{1.0f},
          rendering::mesh::segmentationLabelMeshStyle(
            labelValue,
            normalizedLabelColor(*labelTable, labelIndex),
            labelState));
        renderable.drawOptions.clipPlanes = clipPlanes;
        renderables.push_back(std::move(renderable));
      }
    }
  }
  if (!rendersIsosurfaces(view.renderMode()) && !rendersSegmentations(view.renderMode())) {
    return std::nullopt;
  }

  const rendering::mesh::MeshRenderList list = rendering::mesh::buildRenderList(renderables);
  if (rendering::mesh::visibleRenderableCount(list) == 0u) {
    return std::nullopt;
  }

  const glm::vec3 worldRayOrigin = helper::world_T_ndc(view.threeDCamera(), glm::vec3{viewClipPos, -1.0f});
  const glm::vec3 worldRayDirection = helper::worldRayDirection(view.threeDCamera(), viewClipPos);
  const std::optional<rendering::mesh::MeshScenePickHit> hit = rendering::mesh::pickNearestRenderable(
    {.worldRay = {.origin = worldRayOrigin, .direction = worldRayDirection},
     .renderables = renderables,
     .meshLookup = [this](const rendering::mesh::MeshHandle& handle) {
       return meshDataForHandle(handle, m_meshHandles, m_meshCpuCache);
     }});

  return hit ? std::optional<glm::vec3>{hit->triangleHit.worldPosition} : std::nullopt;
}
