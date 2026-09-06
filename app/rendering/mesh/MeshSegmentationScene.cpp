#include "rendering/Rendering.h"

#include "image/Image.h"
#include "logic/app/Data.h"
#include "logic/app/ParcellationLabelTable.h"
#include "rendering/PrivateMethods.h"
#include "rendering/mesh/MeshExtractionJobs.h"
#include "rendering/mesh/MeshGeneration.h"
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
#include <chrono>
#include <exception>
#include <format>
#include <future>
#include <iterator>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace
{

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

const rendering::mesh::SegmentationLabelInventory* Rendering::presentSegmentationLabels(
  const uuids::uuid& segmentationUid,
  const Image& segmentation,
  const uint32_t timePoint)
{
  const rendering::mesh::SegmentationSourceIdentity currentIdentity{
    .pixelDataRevision = segmentation.pixelDataRevision(),
    .geometryRevision = segmentation.geometryRevision(),
    .timePoint = timePoint};
  if (const auto existing = m_segmentationLabelInventories.find(segmentationUid);
      existing != m_segmentationLabelInventories.end() &&
      rendering::mesh::sameSegmentationValues(existing->second.identity, currentIdentity))
  {
    return &existing->second.labels;
  }

  if (auto pending = m_pendingSegmentationLabelInventories.find(segmentationUid);
      pending != m_pendingSegmentationLabelInventories.end())
  {
    if (pending->second.future.wait_for(std::chrono::seconds{0}) != std::future_status::ready) {
      return nullptr;
    }

    std::optional<rendering::mesh::SegmentationLabelInventory> labels;
    try {
      labels = pending->second.future.get();
    }
    catch (const std::exception& exception) {
      spdlog::error("Unable to inventory segmentation labels for {}: {}", segmentationUid, exception.what());
      m_pendingSegmentationLabelInventories.erase(pending);
      return nullptr;
    }
    catch (...) {
      spdlog::error("Unable to inventory segmentation labels for {}: unknown exception", segmentationUid);
      m_pendingSegmentationLabelInventories.erase(pending);
      return nullptr;
    }
    const rendering::mesh::SegmentationSourceIdentity snapshotIdentity = pending->second.identity;
    const bool isCurrent = rendering::mesh::sameSegmentationValues(snapshotIdentity, currentIdentity);
    std::shared_ptr<const Image> snapshot = std::move(pending->second.snapshot);
    m_pendingSegmentationLabelInventories.erase(pending);
    if (isCurrent && labels) {
      auto [inventory, inserted] = m_segmentationLabelInventories.insert_or_assign(
        segmentationUid,
        SegmentationLabelInventory{
          .identity = snapshotIdentity,
          .labels = std::move(*labels),
          .snapshot = std::move(snapshot)});
      static_cast<void>(inserted);
      return &inventory->second.labels;
    }
  }

  std::shared_ptr<const Image> snapshot = std::make_shared<Image>(segmentation);
  m_pendingSegmentationLabelInventories.insert_or_assign(
    segmentationUid,
    PendingSegmentationLabelInventory{
      .identity = currentIdentity,
      .snapshot = snapshot,
      .future = std::async(std::launch::async, [snapshot, timePoint] {
        return rendering::mesh::segmentationLabelInventory(*snapshot, 0, timePoint);
      })});
  return nullptr;
}

bool Rendering::renderSegmentationMeshesForView(
  const View& view,
  std::vector<rendering::mesh::MeshRenderable>* accumulatedRenderables)
{
  consumeCompletedMeshExtractions();

  const CurrentImages imageSegPairs = meshSceneImagesForView(view);
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
    const rendering::mesh::SegmentationLabelInventory* presentLabels =
      presentSegmentationLabels(segUid, *seg, timePoint);
    if (!presentLabels) {
      continue;
    }
    float imageOpacity = 1.0f;
    if (imgSegPair.first) {
      if (const Image* image = m_appData.image(*imgSegPair.first)) {
        imageOpacity = static_cast<float>(image->settings().opacity());
      }
    }
    const float segmentationOpacity = rendering::mesh::segmentationMeshOpacity(
      static_cast<float>(seg->settings().opacity()),
      imageOpacity,
      m_appData.renderSettings().m_modulateSegmentationOpacityWithImageOpacity3d);
    const rendering::mesh::MeshGenerationOptions generationOptions{
      .threadCount = 0,
      .smoothSurface = m_appData.renderSettings().m_smoothSegmentationMeshes,
      .smoothingIterations = m_appData.renderSettings().m_meshSmoothingIterations,
      .smoothingPassBand = m_appData.renderSettings().m_meshSmoothingPassBand};
    renderables.reserve(renderables.size() + labelTable->numLabels());
    std::shared_ptr<const Image> segmentationSnapshot;
    bool snapshotNeededForFutureSubmission = false;

    for (std::size_t labelIndex = 1; labelIndex < labelTable->numLabels(); ++labelIndex) {
      const int64_t labelValue = static_cast<int64_t>(labelIndex);
      const auto labelInfo = presentLabels->find(labelValue);
      if (labelInfo == presentLabels->end()) {
        continue;
      }
      const rendering::mesh::SegmentationLabelMeshState labelState{
        .showMesh = labelTable->getShowMesh(labelIndex),
        .opacity = segmentationOpacity,
        .hasSharedBoundary = labelInfo->second.hasSharedBoundary};
      if (!rendering::mesh::shouldRenderSegmentationLabelMesh(labelState)) {
        continue;
      }
      const rendering::mesh::SegmentationMeshRequest request = rendering::mesh::makeScalarGridSegmentationRequest(
        segUid,
        seg->pixelDataRevision(),
        seg->geometryRevision(),
        labelValue,
        timePoint,
        generationOptions);
      const rendering::mesh::MeshGeometryKey key = rendering::mesh::geometryKeyForRequest(request);
      const rendering::mesh::MeshHandle handle = m_meshResources.handleFor(key);

      const rendering::mesh::MeshData* readyMesh = m_meshExtractions.readyMesh(key);
      if (!readyMesh) {
        if (m_meshExtractions.canSubmit(key)) {
          if (!segmentationSnapshot) {
            const auto inventory = m_segmentationLabelInventories.find(segUid);
            if (
              inventory != m_segmentationLabelInventories.end() && inventory->second.snapshot &&
              inventory->second.identity == rendering::mesh::SegmentationSourceIdentity{
                                              .pixelDataRevision = seg->pixelDataRevision(),
                                              .geometryRevision = seg->geometryRevision(),
                                              .timePoint = timePoint})
            {
              segmentationSnapshot = inventory->second.snapshot;
            }
            else {
              segmentationSnapshot = std::make_shared<Image>(*seg);
              if (inventory != m_segmentationLabelInventories.end()) {
                inventory->second.identity.geometryRevision = seg->geometryRevision();
                inventory->second.snapshot = segmentationSnapshot;
              }
            }
          }

          const std::string description = segmentationMeshDescription(*seg, *labelTable, labelIndex);
          if (!m_meshExtractions.submit(
                key,
                description,
                rendering::mesh::makeSegmentationExtractionJob(request, labelInfo->second, segmentationSnapshot)))
          {
            snapshotNeededForFutureSubmission = true;
          }
        }
        else if (m_meshExtractions.needsExtraction(key)) {
          snapshotNeededForFutureSubmission = true;
        }

        continue;
      }

      const rendering::mesh::MeshGpuSyncStatus syncStatus = m_meshResources.synchronize(handle, *readyMesh);
      if (
        syncStatus != rendering::mesh::MeshGpuSyncStatus::Uploaded &&
        syncStatus != rendering::mesh::MeshGpuSyncStatus::AlreadyCurrent)
      {
        continue;
      }

      const rendering::mesh::SegmentationLabelMeshStyle style = rendering::mesh::segmentationLabelMeshStyle(
        labelValue,
        normalizedLabelColor(*labelTable, labelIndex),
        labelState,
        m_appData.renderSettings().m_meshSurfaceMaterialSettings);
      rendering::mesh::MeshRenderable renderable =
        rendering::mesh::makeSegmentationLabelRenderable(handle, seg->transformations().worldDef_T_subject(), style);
      renderable.drawOptions.clipPlanes = clipPlanes;
      renderables.push_back(std::move(renderable));
    }

    if (!snapshotNeededForFutureSubmission) {
      const auto inventory = m_segmentationLabelInventories.find(segUid);
      if (inventory != m_segmentationLabelInventories.end()) {
        inventory->second.snapshot.reset();
      }
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
