#include "rendering/mesh/MeshExtractionRunner.h"

#include <optional>
#include <string>
#include <utility>

namespace rendering::mesh
{

namespace
{

MeshExtractionRunResult finishExtraction(
  const MeshGeometryKey& expectedKey,
  std::optional<MeshExtractionResult> extractionResult,
  MeshCache& cache)
{
  if (!extractionResult) {
    std::vector<std::string> diagnostics{"Extractor returned no mesh"};
    const bool accepted = cache.storeFailedIfPending(expectedKey, diagnostics);
    return MeshExtractionRunResult{
      .key = expectedKey,
      .status = accepted ? MeshExtractionRunStatus::Failed : MeshExtractionRunStatus::Stale,
      .diagnostics = std::move(diagnostics)};
  }

  if (extractionResult->key != expectedKey) {
    std::vector<std::string> diagnostics{"Extractor returned a mesh for a different geometry key"};
    const bool accepted = cache.storeFailedIfPending(expectedKey, diagnostics);
    return MeshExtractionRunResult{
      .key = expectedKey,
      .status = accepted ? MeshExtractionRunStatus::Failed : MeshExtractionRunStatus::Stale,
      .diagnostics = std::move(diagnostics)};
  }

  const std::vector<std::string> diagnostics = extractionResult->diagnostics;
  const bool accepted = cache.storeReadyIfPending(std::move(*extractionResult));
  return MeshExtractionRunResult{
    .key = expectedKey,
    .status = accepted ? MeshExtractionRunStatus::Ready : MeshExtractionRunStatus::Stale,
    .diagnostics = diagnostics};
}

} // namespace

MeshExtractionRunResult
runIsosurfaceExtraction(const IsosurfaceMeshRequest& request, IIsosurfaceMeshExtractor& extractor, MeshCache& cache)
{
  const MeshGeometryKey key = geometryKeyForRequest(request);
  cache.markPending(key);
  return finishExtraction(key, extractor.extract(request), cache);
}

MeshExtractionRunResult runSegmentationExtraction(
  const SegmentationMeshRequest& request,
  ISegmentationMeshExtractor& extractor,
  MeshCache& cache)
{
  const MeshGeometryKey key = geometryKeyForRequest(request);
  cache.markPending(key);
  return finishExtraction(key, extractor.extract(request), cache);
}

} // namespace rendering::mesh
