#pragma once

#include "rendering/mesh/MeshExtraction.h"
#include "rendering/mesh/MeshExtractionQueue.h"
#include "rendering/mesh/MeshGeneration.h"
#include "rendering/mesh/MeshImageAdapter.h"

#include <memory>

class Image;

namespace rendering::mesh
{

/** Build a CPU-only isosurface job from an immutable image snapshot. */
MeshExtractionJob makeIsosurfaceExtractionJob(
  IsosurfaceMeshRequest request,
  MeshGenerationOptions options,
  std::shared_ptr<const Image> imageSnapshot);

/** Build a CPU-only cropped binary-label extraction job from an immutable segmentation snapshot. */
MeshExtractionJob makeSegmentationExtractionJob(
  SegmentationMeshRequest request,
  SegmentationLabelBounds bounds,
  MeshGenerationOptions options,
  std::shared_ptr<const Image> segmentationSnapshot);

} // namespace rendering::mesh
