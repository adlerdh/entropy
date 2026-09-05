#include "rendering/mesh/SegmentationExtractionBatch.h"

#include "image/Image.h"
#include "rendering/mesh/MeshImageAdapter.h"

#include <utility>

namespace rendering::mesh
{

SegmentationExtractionBatch::SegmentationExtractionBatch(
  std::shared_ptr<const Image> segmentationSnapshot,
  const uint32_t timePoint,
  const MeshGenerationOptions& options)
  : m_segmentationSnapshot(std::move(segmentationSnapshot)), m_timePoint(timePoint), m_options(options)
{
}

void SegmentationExtractionBatch::generate()
{
  if (!m_segmentationSnapshot) {
    m_diagnostic = "The segmentation snapshot is unavailable";
    return;
  }

  std::optional<PackedSegmentationGrid> packed = packedSegmentationGridFromImageComponent(
    *m_segmentationSnapshot,
    0,
    m_timePoint,
    MeshCoordinateSpace::ImageSubject);
  if (!packed) {
    m_diagnostic = "No shared multi-label segmentation grid could be created";
    return;
  }

  std::optional<SegmentationLabelMeshes> meshes =
    generatePackedSegmentationLabelSurfaces(packed->grid, packed->labelValues, m_options);
  if (!meshes) {
    m_diagnostic = "The segmentation produced no shared label surfaces";
    return;
  }
  m_meshes = std::move(*meshes);
  m_generationSucceeded = true;
}

std::optional<MeshData> SegmentationExtractionBatch::takeLabelMesh(const int64_t labelValue)
{
  std::call_once(m_generateOnce, [this] { generate(); });
  std::scoped_lock lock(m_resultMutex);
  const auto mesh = m_meshes.find(labelValue);
  if (mesh == m_meshes.end()) {
    return std::nullopt;
  }
  std::optional<MeshData> result{std::move(mesh->second)};
  m_meshes.erase(mesh);
  return result;
}

std::string SegmentationExtractionBatch::diagnostic() const
{
  std::scoped_lock lock(m_resultMutex);
  return m_diagnostic.empty() ? "The segmentation label produced no surface triangles" : m_diagnostic;
}

bool SegmentationExtractionBatch::generationSucceeded() const
{
  std::scoped_lock lock(m_resultMutex);
  return m_generationSucceeded;
}

} // namespace rendering::mesh
