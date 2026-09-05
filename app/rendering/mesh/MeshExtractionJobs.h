#pragma once

#include "rendering/mesh/MeshExtraction.h"
#include "rendering/mesh/MeshExtractionQueue.h"
#include "rendering/mesh/MeshGeneration.h"
#include "rendering/mesh/SegmentationExtractionBatch.h"

#include <memory>

class Image;

namespace rendering::mesh
{

/** Build a CPU-only isosurface job from an immutable image snapshot. */
MeshExtractionJob makeIsosurfaceExtractionJob(
  IsosurfaceMeshRequest request,
  const MeshGenerationOptions& options,
  std::shared_ptr<const Image> imageSnapshot);

/** Build a CPU-only label extraction job backed by one shared multi-label segmentation batch. */
MeshExtractionJob makeSegmentationExtractionJob(
  SegmentationMeshRequest request,
  std::shared_ptr<SegmentationExtractionBatch> batch);

} // namespace rendering::mesh
