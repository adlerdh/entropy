#include "rendering/mesh/MeshExtractionJobs.h"

#include "image/Image.h"
#include "rendering/mesh/MeshImageAdapter.h"

#include <algorithm>
#include <ranges>
#include <string>
#include <utility>

namespace rendering::mesh
{

MeshExtractionJob makeIsosurfaceExtractionJob(
  IsosurfaceMeshRequest request,
  const MeshGenerationOptions& options,
  std::shared_ptr<const Image> imageSnapshot)
{
  const MeshGeometryKey key = geometryKeyForRequest(request);
  return [request = std::move(request), key, options, imageSnapshot = std::move(imageSnapshot)]() mutable {
    if (!imageSnapshot) {
      return MeshExtractionJobResult{
        .key = key,
        .result = std::nullopt,
        .diagnostics = {"The source image snapshot is unavailable"}};
    }
    std::optional<ScalarGrid3D> grid = scalarGridFromImageComponent(
      *imageSnapshot,
      request.component,
      request.timePoint,
      MeshCoordinateSpace::ImageSubject);
    if (!grid) {
      return MeshExtractionJobResult{
        .key = key,
        .result = std::nullopt,
        .diagnostics = {"No isosurface scalar grid could be created"}};
    }

    const auto [minValue, maxValue] = std::ranges::minmax(grid->values);
    if (minValue == maxValue || request.isoValue < minValue || request.isoValue > maxValue) {
      return MeshExtractionJobResult{
        .key = key,
        .result = std::nullopt,
        .empty = true,
        .diagnostics = {"The requested isovalue does not intersect the scalar range"}};
    }

    std::optional<MeshData> mesh = generateIsoSurfaceMesh(*grid, request.isoValue, options);
    if (!mesh) {
      return MeshExtractionJobResult{
        .key = key,
        .result = std::nullopt,
        .empty = true,
        .diagnostics = {"The requested isovalue produced no surface triangles"}};
    }
    return MeshExtractionJobResult{
      .key = key,
      .result = MeshExtractionResult{.key = key, .mesh = std::move(*mesh), .diagnostics = {}},
      .diagnostics = {}};
  };
}

MeshExtractionJob makeSegmentationExtractionJob(
  SegmentationMeshRequest request,
  std::shared_ptr<SegmentationExtractionBatch> batch)
{
  const MeshGeometryKey key = geometryKeyForRequest(request);
  return [request = std::move(request), key, batch = std::move(batch)]() mutable {
    if (!batch) {
      return MeshExtractionJobResult{
        .key = key,
        .result = std::nullopt,
        .diagnostics = {"The shared segmentation extraction batch is unavailable"}};
    }

    std::optional<MeshData> mesh = batch->takeLabelMesh(request.labelValue);
    if (!mesh) {
      return MeshExtractionJobResult{
        .key = key,
        .result = std::nullopt,
        .empty = batch->generationSucceeded(),
        .diagnostics = {batch->diagnostic()}};
    }
    return MeshExtractionJobResult{
      .key = key,
      .result = MeshExtractionResult{.key = key, .mesh = std::move(*mesh), .diagnostics = {}},
      .diagnostics = {}};
  };
}

} // namespace rendering::mesh
