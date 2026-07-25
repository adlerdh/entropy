#pragma once

#include "rendering/mesh/MeshCache.h"
#include "rendering/mesh/MeshExtraction.h"

#include <string>
#include <vector>

namespace rendering::mesh
{

/**
 * @brief Status returned after running one extraction request
 */
enum class MeshExtractionRunStatus
{
  Ready,
  Failed,
  Stale
};

/**
 * @brief Summary of an extraction run and cache update
 */
struct MeshExtractionRunResult
{
  MeshGeometryKey key;                                              //!< Geometry key for the request
  MeshExtractionRunStatus status = MeshExtractionRunStatus::Failed; //!< Final run status
  std::vector<std::string> diagnostics;                             //!< Diagnostics returned by the extractor or runner
};

/**
 * @brief Run one isosurface extraction and update the CPU mesh cache
 * @param request Isosurface request
 * @param extractor Extraction backend
 * @param cache Cache receiving pending, ready, failed, or stale state
 * @return Run summary
 * @throw Propagates backend exceptions and allocation failures
 */
MeshExtractionRunResult
runIsosurfaceExtraction(const IsosurfaceMeshRequest& request, IIsosurfaceMeshExtractor& extractor, MeshCache& cache);

/**
 * @brief Run one segmentation-label extraction and update the CPU mesh cache
 * @param request Segmentation-label request
 * @param extractor Extraction backend
 * @param cache Cache receiving pending, ready, failed, or stale state
 * @return Run summary
 * @throw Propagates backend exceptions and allocation failures
 */
MeshExtractionRunResult runSegmentationExtraction(
  const SegmentationMeshRequest& request,
  ISegmentationMeshExtractor& extractor,
  MeshCache& cache);

} // namespace rendering::mesh
