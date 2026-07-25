#include "rendering/mesh/MeshScalarGrid.h"

#include <glm/geometric.hpp>
#include <glm/vec4.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <utility>

namespace rendering::mesh
{
namespace
{

struct TetraVertex
{
  glm::vec3 position;
  float value = 0.0f;
};

constexpr std::array<std::array<int, 4>, 6> kCubeTetrahedra{{
  {{0, 5, 1, 6}},
  {{0, 1, 2, 6}},
  {{0, 2, 3, 6}},
  {{0, 3, 7, 6}},
  {{0, 7, 4, 6}},
  {{0, 4, 5, 6}},
}};

constexpr std::array<glm::uvec3, 8> kCubeOffsets{{
  {0, 0, 0},
  {1, 0, 0},
  {1, 1, 0},
  {0, 1, 0},
  {0, 0, 1},
  {1, 0, 1},
  {1, 1, 1},
  {0, 1, 1},
}};

glm::vec3 transformGridPoint(const glm::mat4& grid_T_voxelIndex, const glm::vec3& voxelIndex)
{
  const glm::vec4 point{voxelIndex, 1.0f};
  const glm::vec4 transformed = grid_T_voxelIndex * point;

  if (transformed.w == 0.0f) {
    return glm::vec3{transformed};
  }

  return glm::vec3{transformed} / transformed.w;
}

glm::vec3 transformGridPoint(const glm::mat4& grid_T_voxelIndex, const glm::uvec3& voxelIndex)
{
  return transformGridPoint(
    grid_T_voxelIndex,
    glm::vec3{static_cast<float>(voxelIndex.x), static_cast<float>(voxelIndex.y), static_cast<float>(voxelIndex.z)});
}

float sampleGridValue(const ScalarGrid3D& grid, const glm::uvec3& voxelIndex)
{
  return grid.values[scalarGridValueIndex(grid.dimensions, voxelIndex.x, voxelIndex.y, voxelIndex.z)];
}

std::optional<glm::vec3> edgeIntersection(const TetraVertex& a, const TetraVertex& b, double isoValue)
{
  const double aValue = static_cast<double>(a.value);
  const double bValue = static_cast<double>(b.value);
  const double denom = bValue - aValue;

  if (denom == 0.0) {
    return std::nullopt;
  }

  const double t = (isoValue - aValue) / denom;
  if (t < 0.0 || t > 1.0) {
    return std::nullopt;
  }

  return a.position + static_cast<float>(t) * (b.position - a.position);
}

std::vector<glm::vec3> tetraIntersections(const std::array<TetraVertex, 4>& tetra, double isoValue)
{
  std::vector<glm::vec3> intersections;
  intersections.reserve(4);

  constexpr std::array<std::pair<int, int>, 6> kEdges{{{0, 1}, {0, 2}, {0, 3}, {1, 2}, {1, 3}, {2, 3}}};

  for (const auto [a, b] : kEdges) {
    const bool aBelow = static_cast<double>(tetra[a].value) < isoValue;
    const bool bBelow = static_cast<double>(tetra[b].value) < isoValue;
    if (aBelow == bBelow) {
      continue;
    }

    if (const std::optional<glm::vec3> point = edgeIntersection(tetra[a], tetra[b], isoValue)) {
      intersections.push_back(*point);
    }
  }

  return intersections;
}

void appendTriangle(MeshData& mesh, glm::vec3 a, glm::vec3 b, glm::vec3 c, const glm::vec3& gridCenter)
{
  glm::vec3 normal = glm::cross(b - a, c - a);
  const float normalLength = glm::length(normal);

  if (normalLength == 0.0f) {
    return;
  }

  normal /= normalLength;

  const glm::vec3 centroid = (a + b + c) / 3.0f;
  if (glm::dot(normal, centroid - gridCenter) < 0.0f) {
    std::swap(b, c);
    normal = -normal;
  }

  const uint32_t firstIndex = static_cast<uint32_t>(mesh.positions.size());
  mesh.positions.push_back(a);
  mesh.positions.push_back(b);
  mesh.positions.push_back(c);
  mesh.normals.push_back(normal);
  mesh.normals.push_back(normal);
  mesh.normals.push_back(normal);
  mesh.indices.push_back(firstIndex);
  mesh.indices.push_back(firstIndex + 1);
  mesh.indices.push_back(firstIndex + 2);
}

void appendTetraSurface(
  MeshData& mesh,
  const std::array<TetraVertex, 4>& tetra,
  double isoValue,
  const glm::vec3& gridCenter)
{
  const std::vector<glm::vec3> points = tetraIntersections(tetra, isoValue);

  if (points.size() == 3) {
    appendTriangle(mesh, points[0], points[1], points[2], gridCenter);
    return;
  }

  if (points.size() == 4) {
    appendTriangle(mesh, points[0], points[1], points[2], gridCenter);
    appendTriangle(mesh, points[0], points[2], points[3], gridCenter);
  }
}

ScalarGrid3D binaryGridForLabel(const ScalarGrid3D& labelGrid, int64_t labelValue)
{
  ScalarGrid3D binary = labelGrid;

  std::ranges::transform(labelGrid.values, binary.values.begin(), [labelValue](float value) {
    return static_cast<int64_t>(std::llround(value)) == labelValue ? 1.0f : 0.0f;
  });

  return binary;
}

} // namespace

bool isValidScalarGrid(const ScalarGrid3D& grid)
{
  if (grid.dimensions.x < 2 || grid.dimensions.y < 2 || grid.dimensions.z < 2) {
    return false;
  }

  const std::size_t expectedSize = static_cast<std::size_t>(grid.dimensions.x) * grid.dimensions.y * grid.dimensions.z;
  return grid.values.size() == expectedSize;
}

std::size_t scalarGridValueIndex(const glm::uvec3& dimensions, uint32_t i, uint32_t j, uint32_t k)
{
  return static_cast<std::size_t>(i) + static_cast<std::size_t>(dimensions.x) *
                                         (static_cast<std::size_t>(j) + static_cast<std::size_t>(dimensions.y) * k);
}

std::optional<MeshData> extractIsosurfaceMesh(const ScalarGrid3D& grid, double isoValue)
{
  if (!isValidScalarGrid(grid)) {
    return std::nullopt;
  }

  MeshData mesh;
  mesh.coordinateSpace = grid.coordinateSpace;

  const glm::vec3 gridCenter =
    transformGridPoint(grid.grid_T_voxelIndex, 0.5f * glm::vec3{grid.dimensions - glm::uvec3{1}});

  for (uint32_t k = 0; k + 1 < grid.dimensions.z; ++k) {
    for (uint32_t j = 0; j + 1 < grid.dimensions.y; ++j) {
      for (uint32_t i = 0; i + 1 < grid.dimensions.x; ++i) {
        std::array<TetraVertex, 8> cube{};
        for (std::size_t vertex = 0; vertex < cube.size(); ++vertex) {
          const glm::uvec3 voxelIndex = glm::uvec3{i, j, k} + kCubeOffsets[vertex];
          cube[vertex] = {
            .position = transformGridPoint(grid.grid_T_voxelIndex, voxelIndex),
            .value = sampleGridValue(grid, voxelIndex)};
        }

        for (const std::array<int, 4>& tetraIndices : kCubeTetrahedra) {
          appendTetraSurface(
            mesh,
            {cube[tetraIndices[0]], cube[tetraIndices[1]], cube[tetraIndices[2]], cube[tetraIndices[3]]},
            isoValue,
            gridCenter);
        }
      }
    }
  }

  if (mesh.positions.empty()) {
    return std::nullopt;
  }

  return mesh;
}

std::optional<MeshData> extractSegmentationLabelMesh(const ScalarGrid3D& grid, int64_t labelValue)
{
  if (!isValidScalarGrid(grid)) {
    return std::nullopt;
  }

  return extractIsosurfaceMesh(binaryGridForLabel(grid, labelValue), 0.5);
}

ScalarGridIsosurfaceExtractor::ScalarGridIsosurfaceExtractor(IsosurfaceScalarGridProvider provider)
  : m_provider(std::move(provider))
{
}

std::optional<MeshExtractionResult> ScalarGridIsosurfaceExtractor::extract(const IsosurfaceMeshRequest& request)
{
  if (!m_provider) {
    return std::nullopt;
  }

  const std::optional<ScalarGrid3D> grid = m_provider(request);
  if (!grid) {
    return std::nullopt;
  }

  std::optional<MeshData> mesh = extractIsosurfaceMesh(*grid, request.isoValue);
  if (!mesh) {
    return std::nullopt;
  }

  return MeshExtractionResult{.key = geometryKeyForRequest(request), .mesh = std::move(*mesh), .diagnostics = {}};
}

ScalarGridSegmentationExtractor::ScalarGridSegmentationExtractor(SegmentationScalarGridProvider provider)
  : m_provider(std::move(provider))
{
}

std::optional<MeshExtractionResult> ScalarGridSegmentationExtractor::extract(const SegmentationMeshRequest& request)
{
  if (!m_provider) {
    return std::nullopt;
  }

  const std::optional<ScalarGrid3D> grid = m_provider(request);
  if (!grid) {
    return std::nullopt;
  }

  std::optional<MeshData> mesh = extractSegmentationLabelMesh(*grid, request.labelValue);
  if (!mesh) {
    return std::nullopt;
  }

  return MeshExtractionResult{.key = geometryKeyForRequest(request), .mesh = std::move(*mesh), .diagnostics = {}};
}

} // namespace rendering::mesh
