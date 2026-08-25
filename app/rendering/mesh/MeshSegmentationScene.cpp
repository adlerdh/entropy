#include "rendering/Rendering.h"

#include "common/UuidUtility.h"
#include "image/Image.h"
#include "logic/app/Data.h"
#include "logic/app/ParcellationLabelTable.h"
#include "rendering/PrivateMethods.h"
#include "rendering/mesh/MeshExtractionQueue.h"
#include "rendering/mesh/MeshGeneration.h"
#include "rendering/mesh/MeshGpuSync.h"
#include "rendering/mesh/MeshImageAdapter.h"
#include "rendering/mesh/MeshRenderableFactory.h"
#include "rendering/mesh/MeshScene.h"
#include "rendering/mesh/MeshSegmentationPolicy.h"
#include "windowing/View.h"

#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>
#include <spdlog/spdlog.h>

#include <cstddef>
#include <cstdint>
#include <format>
#include <iterator>
#include <memory>
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

MeshHandle meshHandleForKey(const MeshGeometryKey& key, MeshHandleMap& handles)
{
  if (const auto existing = handles.find(key); existing != handles.end()) {
    return existing->second;
  }

  MeshHandle handle{.uid = generateRandomUuid(), .geometryVersion = MeshGeometryKeyHash{}(key)};
  handles.emplace(key, handle);
  return handle;
}

glm::vec4 normalizedLabelColor(const ParcellationLabelTable& labelTable, const std::size_t labelIndex)
{
  return glm::vec4{glm::vec3{labelTable.getColor(labelIndex)}, static_cast<float>(labelTable.getAlpha(labelIndex))} /
         255.0f;
}

std::string segmentationMeshDescription(
  const Image& segmentation,
  const ParcellationLabelTable& labelTable,
  const std::size_t labelIndex)
{
  return std::format(
    "Segmentation mesh: {} - {} (label {})",
    segmentation.settings().displayName(),
    labelTable.getName(labelIndex),
    labelIndex);
}

} // namespace

const rendering::mesh::SegmentationLabelSet* Rendering::presentSegmentationLabels(
  const uuids::uuid& segmentationUid,
  const Image& segmentation,
  const uint32_t timePoint)
{
  const uint64_t pixelDataRevision = segmentation.pixelDataRevision();
  if (const auto existing = m_segmentationLabelInventories.find(segmentationUid);
      existing != m_segmentationLabelInventories.end() && existing->second.pixelDataRevision == pixelDataRevision &&
      existing->second.timePoint == timePoint)
  {
    return &existing->second.presentLabelValues;
  }

  std::optional<rendering::mesh::SegmentationLabelSet> labels =
    rendering::mesh::presentSegmentationLabels(segmentation, 0, timePoint);
  if (!labels) {
    return nullptr;
  }

  auto [inventory, inserted] = m_segmentationLabelInventories.insert_or_assign(
    segmentationUid,
    SegmentationLabelInventory{
      .pixelDataRevision = pixelDataRevision,
      .timePoint = timePoint,
      .presentLabelValues = std::move(*labels)});
  static_cast<void>(inserted);
  return &inventory->second.presentLabelValues;
}

bool Rendering::renderSegmentationMeshesForView(
  const View& view,
  std::vector<rendering::mesh::MeshRenderable>* accumulatedRenderables)
{
  consumeCompletedMeshExtractions();

  const CurrentImages imageSegPairs = raycastImagesForView(view);
  if (imageSegPairs.empty()) {
    return false;
  }

  const std::vector<rendering::mesh::MeshClipPlane> clipPlanes = meshClipPlanes();
  std::vector<rendering::mesh::MeshRenderable> renderables;
  std::vector<rendering::mesh::MeshRenderable> imagePlaneBorderRenderables;
  std::vector<rendering::mesh::MeshImagePlaneRenderable> imagePlaneRenderables;
  if (!accumulatedRenderables) {
    imagePlaneRenderables = collectMeshImagePlaneRenderablesForView(view, imagePlaneBorderRenderables);
  }

  for (const ImgSegPair& imgSegPair : imageSegPairs) {
    if (!imgSegPair.second) {
      continue;
    }

    const uuids::uuid& segUid = *imgSegPair.second;
    const Image* seg = m_appData.seg(segUid);
    if (!seg || !seg->settings().visibility()) {
      continue;
    }

    const auto tableUid = m_appData.labelTableUid(seg->settings().labelTableIndex());
    const ParcellationLabelTable* labelTable = tableUid ? m_appData.labelTable(*tableUid) : nullptr;
    if (!labelTable) {
      spdlog::warn("Unable to render segmentation meshes for {}: missing label table", segUid);
      continue;
    }

    const uint32_t timePoint = seg->timeAxis().clamp(seg->settings().activeTimePoint());
    const rendering::mesh::SegmentationLabelSet* presentLabels = presentSegmentationLabels(segUid, *seg, timePoint);
    if (!presentLabels) {
      continue;
    }
    const float segmentationOpacity = static_cast<float>(seg->settings().opacity());
    renderables.reserve(renderables.size() + labelTable->numLabels());
    std::shared_ptr<const Image> segmentationSnapshot;

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
      const rendering::mesh::MeshGenerationOptions generationOptions{
        .threadCount = m_appData.renderData().m_meshGenerationThreadCount};
      const rendering::mesh::MeshGeometryKey key = rendering::mesh::geometryKeyForRequest(request);
      const rendering::mesh::MeshHandle handle = meshHandleForKey(key, m_meshHandles);

      if (!m_meshCpuCache.readyMesh(key)) {
        if (!m_meshCpuCache.contains(key) && !m_meshExtractionQueue.active(key)) {
          if (!segmentationSnapshot) {
            segmentationSnapshot = std::make_shared<Image>(*seg);
          }

          const std::string description = segmentationMeshDescription(*seg, *labelTable, labelIndex);
          if (m_meshExtractionQueue
                .submit(key, description, [request, key, generationOptions, segmentationSnapshot]() mutable {
                  std::optional<rendering::mesh::ScalarGrid3D> grid = rendering::mesh::labelMaskGridFromImageComponent(
                    *segmentationSnapshot,
                    0,
                    request.labelValue,
                    request.timePoint,
                    rendering::mesh::MeshCoordinateSpace::World);
                  if (!grid) {
                    return rendering::mesh::MeshExtractionJobResult{
                      .key = key,
                      .result = std::nullopt,
                      .diagnostics = {std::string{"No segmentation label grid could be created"}}};
                  }

                  std::optional<rendering::mesh::MeshData> mesh =
                    rendering::mesh::generateLabelMesh(*grid, 1, generationOptions);
                  if (!mesh) {
                    return rendering::mesh::MeshExtractionJobResult{
                      .key = key,
                      .result = std::nullopt,
                      .diagnostics = {std::string{"No segmentation label mesh could be extracted"}}};
                  }

                  return rendering::mesh::MeshExtractionJobResult{
                    .key = key,
                    .result = rendering::mesh::MeshExtractionResult{.key = key, .mesh = std::move(*mesh)}};
                }))
          {
            m_meshCpuCache.markPending(key);
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
        continue;
      }

      const rendering::mesh::SegmentationLabelMeshStyle style = rendering::mesh::segmentationLabelMeshStyle(
        labelValue,
        normalizedLabelColor(*labelTable, labelIndex),
        labelState);
      rendering::mesh::MeshRenderable renderable =
        rendering::mesh::makeSegmentationLabelRenderable(handle, glm::mat4{1.0f}, style);
      renderable.drawOptions.clipPlanes = clipPlanes;
      renderables.push_back(std::move(renderable));
    }
  }

  if (accumulatedRenderables) {
    const bool hasRenderables = !renderables.empty();
    accumulatedRenderables->insert(
      accumulatedRenderables->end(),
      std::make_move_iterator(renderables.begin()),
      std::make_move_iterator(renderables.end()));
    return hasRenderables;
  }

  renderables.insert(renderables.end(), imagePlaneBorderRenderables.begin(), imagePlaneBorderRenderables.end());

  rendering::mesh::MeshScene imagePlaneScene;
  imagePlaneScene.setImagePlaneRenderables(std::move(imagePlaneRenderables));
  const rendering::mesh::MeshImagePlaneRenderList imagePlaneList =
    rendering::mesh::buildImagePlaneRenderList(imagePlaneScene.imagePlaneRenderables());

  // Keep the crosshairs glyph in the same mesh pass as translucent segmentation surfaces so DDP can depth-order the
  // glyph with the surface instead of drawing the glyph later against an empty default depth buffer.
  appendMeshCrosshairsRenderableForView(view, renderables);

  if (renderables.empty() && imagePlaneList.imagePlanes.empty()) {
    return false;
  }

  rendering::mesh::MeshScene scene;
  scene.setRenderables(renderables);
  const rendering::mesh::MeshRenderList list = rendering::mesh::buildRenderList(scene.renderables());
  drawMeshRenderListForView(view, list, &imagePlaneList);
  return true;
}
