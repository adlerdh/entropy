#include "rendering/Rendering.h"

#include "image/Image.h"
#include "logic/app/Data.h"
#include "logic/app/DeformationWarp.h"
#include "logic/app/ImportedMeshWarp.h"
#include "mesh/MeshTransform.h"
#include "mesh/MeshTypes.h"
#include "rendering/PrivateMethods.h"
#include "rendering/mesh/MeshCompositing.h"
#include "rendering/mesh/MeshData.h"
#include "rendering/mesh/MeshRenderable.h"
#include "windowing/View.h"

#include <glm/vec4.hpp>
#include <spdlog/spdlog.h>

#include <bit>
#include <expected>

std::optional<Rendering::PreparedImportedMeshGeometry> Rendering::prepareImportedMeshGeometry(
  const uuids::uuid& imageUid,
  const uuids::uuid& meshUid)
{
  const Image* image = m_appData.image(imageUid);
  const mesh::MeshRecord* imported = m_appData.importedMesh(meshUid);
  if (!image || !imported) {
    return std::nullopt;
  }

  const bool applyForwardWarp = image->settings().warpEnabled() && image->settings().warpStrength() > 0.0f &&
                                m_appData.imageToActiveForwardWarpUid(imageUid).has_value();
  const uint64_t geometryVersion =
    applyForwardWarp ? deformation_warp::warpedGeometryVersion(m_appData, imageUid, *image) : 1;
  auto [cpuIt, inserted] = m_importedMeshData.try_emplace(meshUid);

  if (inserted || m_importedMeshVersions[meshUid] != geometryVersion) {
    if (applyForwardWarp) {
      const auto transformed = deformation_warp::prepareImportedGeometry(m_appData, imageUid, imported->geometry);

      if (!transformed) {
        spdlog::error(
          "Could not apply the image deformation to imported mesh {}: {}",
          meshUid,
          transformed.error().message);
        return std::nullopt;
      }
      cpuIt->second.positions = transformed->positions;
      cpuIt->second.normals = transformed->normals;
      cpuIt->second.indices = transformed->triangleIndices;
      cpuIt->second.coordinateSpace = rendering::mesh::MeshCoordinateSpace::World;
    }
    else {
      cpuIt->second.positions = imported->geometry.positions;
      cpuIt->second.normals = imported->geometry.normals;
      cpuIt->second.indices = imported->geometry.triangleIndices;
      cpuIt->second.coordinateSpace = rendering::mesh::MeshCoordinateSpace::ImageSubject;
    }
    m_importedMeshVersions[meshUid] = geometryVersion;
  }

  return PreparedImportedMeshGeometry{
    .geometry = &cpuIt->second,
    .world_T_mesh = applyForwardWarp ? glm::mat4{1.0f} : image->transformations().worldDef_T_subject(),
    .geometryVersion = geometryVersion};
}

void Rendering::appendImportedMeshesForView(
  const View& view,
  const CurrentImages& imageSegPairs,
  std::vector<rendering::mesh::MeshRenderable>& renderables)
{
  const auto& surfaceSettings = m_appData.renderSettings().m_meshSurfaceMaterialSettings;
  const rendering::mesh::MeshOctantCutaway cutaway = meshCutawayForView(view);

  for (const ImgSegPair& pair : imageSegPairs) {
    if (!pair.first) {
      continue;
    }

    const uuids::uuid& imageUid = *pair.first;
    if (!m_appData.image(imageUid)) {
      continue;
    }

    for (const uuids::uuid& meshUid : m_appData.imageToImportedMeshUids(imageUid)) {
      const mesh::MeshRecord* imported = m_appData.importedMesh(meshUid);
      if (!imported || !imported->display.visibleIn3d || imported->display.opacity <= 0.0f) {
        continue;
      }

      const auto prepared = prepareImportedMeshGeometry(imageUid, meshUid);
      if (!prepared || !prepared->geometry) {
        continue;
      }

      const rendering::mesh::MeshGeometryKey key{
        .sourceUid = meshUid,
        .sourceDataVersion = 1,
        .sourceGeometryVersion = prepared->geometryVersion,
        .component = std::nullopt,
        .labelValue = std::nullopt,
        .timePoint = 0,
        .isoValue = 0.0,
        .extractionAlgorithm = "imported-surface",
        .extractionAlgorithmVersion = 1};
      const rendering::mesh::MeshHandle handle = m_meshResources.handleFor(key);
      const auto syncStatus = m_meshResources.synchronize(handle, *prepared->geometry);

      if (syncStatus == rendering::mesh::MeshGpuSyncStatus::UploadFailed) {
        continue;
      }

      glm::vec4 color{imported->display.baseColor, imported->display.opacity};
      rendering::mesh::MeshRenderable renderable{
        .mesh = handle,
        .world_T_mesh = prepared->world_T_mesh,
        .material = rendering::mesh::meshMaterialForSurface(color, surfaceSettings),
        .compositingMode = rendering::mesh::compositingModeForSurfaceAlpha(
          imported->display.opacity,
          surfaceSettings.rimLightingEnabled && surfaceSettings.rimOpacityStrength > 0.0f),
        .drawOptions = {},
        .visible = true,
        .castsShadow = true};
      renderable.drawOptions.cutaway = cutaway;
      renderables.push_back(std::move(renderable));
    }
  }
}
