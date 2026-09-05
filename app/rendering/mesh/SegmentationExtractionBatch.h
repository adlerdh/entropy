#pragma once

#include "rendering/mesh/MeshData.h"
#include "rendering/mesh/MeshGeneration.h"

#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <string>

class Image;

namespace rendering::mesh
{

/**
 * @brief Lazily generate and distribute meshes from one joint multi-label segmentation extraction
 *
 * Jobs for individual cache keys share this object. The first job performs extraction and smoothing once; subsequent
 * jobs move out their label mesh. This preserves the existing per-label cache API without duplicating shared-boundary
 * extraction work.
 */
class SegmentationExtractionBatch
{
public:
  SegmentationExtractionBatch(
    std::shared_ptr<const Image> segmentationSnapshot,
    uint32_t timePoint,
    const MeshGenerationOptions& options);

  SegmentationExtractionBatch(const SegmentationExtractionBatch&) = delete;
  SegmentationExtractionBatch& operator=(const SegmentationExtractionBatch&) = delete;

  /** Return and remove one generated label mesh. Empty means generation failed or the label had no boundary. */
  std::optional<MeshData> takeLabelMesh(int64_t labelValue);

  /** Return a diagnostic suitable for an individual failed job. */
  std::string diagnostic() const;

  /** Return whether joint extraction completed successfully, even if a requested label had no triangles. */
  bool generationSucceeded() const;

private:
  void generate();

  std::shared_ptr<const Image> m_segmentationSnapshot;
  uint32_t m_timePoint = 0;
  MeshGenerationOptions m_options;
  std::once_flag m_generateOnce;
  mutable std::mutex m_resultMutex;
  SegmentationLabelMeshes m_meshes;
  std::string m_diagnostic;
  bool m_generationSucceeded = false;
};

} // namespace rendering::mesh
