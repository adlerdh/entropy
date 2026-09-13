#include "rendering/mesh/MeshExtractionJobs.h"

#include "image/Image.h"
#include "rendering/mesh/MeshGeneration.h"
#include "rendering/mesh/MeshImageAdapter.h"

#include <algorithm>
#include <ranges>
#include <string>
#include <utility>

namespace rendering::mesh
{

MeshExtractionJob makeIsosurfaceExtractionJob(IsosurfaceMeshRequest request, std::shared_ptr<const Image> imageSnapshot)
{
  const MeshGeometryKey key = geometryKeyForRequest(request);
  return [request = std::move(request), key, imageSnapshot = std::move(imageSnapshot)]() mutable {
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

    std::optional<MeshData> mesh = generateIsoSurfaceMesh(*grid, request.isoValue, request.generationOptions);
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
  SegmentationLabelBounds bounds,
  std::shared_ptr<const Image> segmentationSnapshot)
{
  const MeshGeometryKey key = geometryKeyForRequest(request);
  return [request = std::move(request), key, bounds, segmentationSnapshot = std::move(segmentationSnapshot)]() mutable {
    if (!segmentationSnapshot) {
      return MeshExtractionJobResult{
        .key = key,
        .result = std::nullopt,
        .diagnostics = {"The segmentation snapshot is unavailable"}};
    }

    std::optional<ScalarGrid3D> grid = labelMaskGridFromImageComponent(
      *segmentationSnapshot,
      0,
      request.labelValue,
      bounds,
      request.timePoint,
      MeshCoordinateSpace::ImageSubject);
    if (!grid) {
      return MeshExtractionJobResult{
        .key = key,
        .result = std::nullopt,
        .diagnostics = {"No segmentation label grid could be created"}};
    }

    std::optional<MeshData> mesh = generateBinaryMaskSurface(*grid, request.generationOptions);
    if (!mesh) {
      return MeshExtractionJobResult{
        .key = key,
        .result = std::nullopt,
        .empty = true,
        .diagnostics = {"The segmentation label produced no surface triangles"}};
    }
    return MeshExtractionJobResult{
      .key = key,
      .result = MeshExtractionResult{.key = key, .mesh = std::move(*mesh), .diagnostics = {}},
      .diagnostics = {}};
  };
}

} // namespace rendering::mesh
