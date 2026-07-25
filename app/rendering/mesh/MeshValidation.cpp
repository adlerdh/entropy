#include "rendering/mesh/MeshValidation.h"

#include "rendering/mesh/MeshBounds.h"

#include <algorithm>
#include <cstddef>
#include <ranges>

namespace rendering::mesh
{

std::vector<MeshValidationError> validateMeshData(const MeshData& mesh)
{
  std::vector<MeshValidationError> errors;

  if (mesh.positions.empty()) {
    errors.push_back(MeshValidationError::EmptyPositions);
  }

  if (mesh.indices.empty()) {
    errors.push_back(MeshValidationError::EmptyIndices);
  }

  if (mesh.indices.size() % 3 != 0) {
    errors.push_back(MeshValidationError::IndicesNotTriangles);
  }

  if (!mesh.normals.empty() && mesh.normals.size() != mesh.positions.size()) {
    errors.push_back(MeshValidationError::NormalCountMismatch);
  }

  if (mesh.colors && mesh.colors->size() != mesh.positions.size()) {
    errors.push_back(MeshValidationError::ColorCountMismatch);
  }

  if (mesh.textureCoords && mesh.textureCoords->size() != mesh.positions.size()) {
    errors.push_back(MeshValidationError::TextureCoordCountMismatch);
  }

  if (std::ranges::any_of(mesh.positions, [](const glm::vec3& position) { return !isFinite(position); })) {
    errors.push_back(MeshValidationError::NonFinitePosition);
  }

  if (std::ranges::any_of(mesh.normals, [](const glm::vec3& normal) { return !isFinite(normal); })) {
    errors.push_back(MeshValidationError::NonFiniteNormal);
  }

  if (mesh.colors && std::ranges::any_of(*mesh.colors, [](const glm::vec4& color) { return !isFinite(color); })) {
    errors.push_back(MeshValidationError::NonFiniteColor);
  }

  if (mesh.textureCoords && std::ranges::any_of(*mesh.textureCoords, [](const glm::vec3& textureCoord) {
        return !isFinite(textureCoord);
      }))
  {
    errors.push_back(MeshValidationError::NonFiniteTextureCoord);
  }

  const auto outOfRange = [numPositions = mesh.positions.size()](uint32_t index) {
    return static_cast<size_t>(index) >= numPositions;
  };
  if (std::ranges::any_of(mesh.indices, outOfRange)) {
    errors.push_back(MeshValidationError::IndexOutOfRange);
  }

  return errors;
}

bool isValidMeshData(const MeshData& mesh)
{
  return validateMeshData(mesh).empty();
}

} // namespace rendering::mesh
