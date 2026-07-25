#pragma once

#include "rendering/mesh/MeshData.h"
#include "rendering/mesh/MeshExtraction.h"

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include <cstdint>
#include <functional>
#include <optional>
#include <vector>

namespace rendering::mesh
{

/**
 * @brief Scalar voxel grid used by dependency-free mesh extraction backends
 *
 * The grid stores one scalar value per voxel in i-fastest order. Voxel coordinates are transformed to the target
 * coordinate space using `grid_T_voxelIndex`, where voxel index coordinates are continuous i, j, k locations.
 */
struct ScalarGrid3D
{
  glm::uvec3 dimensions{0, 0, 0};    //!< Number of voxels along i, j, and k
  glm::mat4 grid_T_voxelIndex{1.0f}; //!< Transform from continuous voxel index coordinates to mesh coordinates
  std::vector<float> values;         //!< Scalar samples in i-fastest order
  MeshCoordinateSpace coordinateSpace = MeshCoordinateSpace::ImageSubject; //!< Coordinate space of extracted positions
};

/**
 * @brief Get whether a scalar grid can contain closed 3D cell geometry
 * @param grid Scalar grid
 * @return True when dimensions and sample count are valid for 3D extraction
 */
bool isValidScalarGrid(const ScalarGrid3D& grid);

/**
 * @brief Compute the flat i-fastest scalar index for a voxel location
 * @param dimensions Grid dimensions
 * @param i Voxel i index
 * @param j Voxel j index
 * @param k Voxel k index
 * @return Flat scalar index
 */
std::size_t scalarGridValueIndex(const glm::uvec3& dimensions, uint32_t i, uint32_t j, uint32_t k);

/**
 * @brief Extract an isosurface mesh from a scalar grid with marching tetrahedra
 * @param grid Scalar grid
 * @param isoValue Isovalue
 * @return Triangle mesh, or empty when the grid cannot produce a surface
 */
std::optional<MeshData> extractIsosurfaceMesh(const ScalarGrid3D& grid, double isoValue);

/**
 * @brief Extract a segmentation label mesh from a scalar label grid
 * @param grid Scalar label grid
 * @param labelValue Label value to extract
 * @return Triangle mesh, or empty when the grid cannot produce a surface
 */
std::optional<MeshData> extractSegmentationLabelMesh(const ScalarGrid3D& grid, int64_t labelValue);

/**
 * @brief Callback that provides scalar data for an isosurface extraction request
 */
using IsosurfaceScalarGridProvider = std::function<std::optional<ScalarGrid3D>(const IsosurfaceMeshRequest&)>;

/**
 * @brief Callback that provides label data for a segmentation extraction request
 */
using SegmentationScalarGridProvider = std::function<std::optional<ScalarGrid3D>(const SegmentationMeshRequest&)>;

/**
 * @brief Isosurface extractor that runs a small dependency-free marching tetrahedra backend
 */
class ScalarGridIsosurfaceExtractor : public IIsosurfaceMeshExtractor
{
public:
  /**
   * @brief Create an extractor backed by a scalar-grid provider
   * @param provider Function that returns scalar data for each request
   */
  explicit ScalarGridIsosurfaceExtractor(IsosurfaceScalarGridProvider provider);

  /**
   * @brief Extract a mesh for one isosurface request
   * @param request Request metadata and isovalue
   * @return Extraction result, or empty when no valid grid or surface is available
   */
  std::optional<MeshExtractionResult> extract(const IsosurfaceMeshRequest& request) override;

private:
  IsosurfaceScalarGridProvider m_provider; //!< Source of scalar grids
};

/**
 * @brief Segmentation extractor that runs a small dependency-free marching tetrahedra backend
 */
class ScalarGridSegmentationExtractor : public ISegmentationMeshExtractor
{
public:
  /**
   * @brief Create an extractor backed by a label-grid provider
   * @param provider Function that returns label data for each request
   */
  explicit ScalarGridSegmentationExtractor(SegmentationScalarGridProvider provider);

  /**
   * @brief Extract a mesh for one segmentation-label request
   * @param request Request metadata and label value
   * @return Extraction result, or empty when no valid grid or surface is available
   */
  std::optional<MeshExtractionResult> extract(const SegmentationMeshRequest& request) override;

private:
  SegmentationScalarGridProvider m_provider; //!< Source of label grids
};

} // namespace rendering::mesh
