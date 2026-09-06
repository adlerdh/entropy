#pragma once

#include "rendering/mesh/MeshData.h"

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include <cstdint>
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

} // namespace rendering::mesh
