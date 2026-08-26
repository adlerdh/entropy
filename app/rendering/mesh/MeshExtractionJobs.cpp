#include "rendering/mesh/MeshExtractionJobs.h"

#include "image/Image.h"

#include <algorithm>
#include <ranges>
#include <string>
#include <utility>

namespace rendering::mesh
{

MeshExtractionJob makeIsosurfaceExtractionJob(
  IsosurfaceMeshRequest request,
  MeshGenerationOptions options,
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
    return MeshExtractionJobResult{.key = key, .result = MeshExtractionResult{.key = key, .mesh = std::move(*mesh)}};
  };
}

MeshExtractionJob makeSegmentationExtractionJob(
  SegmentationMeshRequest request,
  SegmentationLabelBounds bounds,
  MeshGenerationOptions options,
  std::shared_ptr<const Image> segmentationSnapshot)
{
  const MeshGeometryKey key = geometryKeyForRequest(request);
  return [request = std::move(request),
          key,
          bounds,
          options,
          segmentationSnapshot = std::move(segmentationSnapshot)]() mutable {
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

    std::optional<MeshData> mesh = generateBinaryMaskSurface(*grid, options);
    if (!mesh) {
      return MeshExtractionJobResult{
        .key = key,
        .result = std::nullopt,
        .empty = true,
        .diagnostics = {"The segmentation label produced no surface triangles"}};
    }
    return MeshExtractionJobResult{.key = key, .result = MeshExtractionResult{.key = key, .mesh = std::move(*mesh)}};
  };
}

} // namespace rendering::mesh
