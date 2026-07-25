#pragma once

#include "rendering/mesh/MeshData.h"

#include <vector>

namespace rendering::mesh
{

/**
 * @brief Mesh validation error detected before GPU upload or picking
 */
enum class MeshValidationError
{
  EmptyPositions,
  EmptyIndices,
  IndicesNotTriangles,
  IndexOutOfRange,
  NonFinitePosition,
  NonFiniteNormal,
  NonFiniteColor,
  NonFiniteTextureCoord,
  NormalCountMismatch,
  ColorCountMismatch,
  TextureCoordCountMismatch
};

/**
 * @brief Validate mesh array sizes, indices, and finite values
 * @param mesh Mesh to validate
 * @return List of validation errors, empty when the mesh is valid
 * @throw Propagates allocation failures from the returned vector
 */
std::vector<MeshValidationError> validateMeshData(const MeshData& mesh);

/**
 * @brief Return true when mesh validation reports no errors
 * @param mesh Mesh to validate
 * @return Whether the mesh is valid
 */
bool isValidMeshData(const MeshData& mesh);

} // namespace rendering::mesh
