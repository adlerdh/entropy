#include "rendering/mesh/MeshValidation.h"

#include "rendering/mesh/MeshBounds.h"

#include <algorithm>
#include <cstddef>
#include <limits>
#include <ranges>

namespace rendering::mesh
{

namespace
{

bool hasDegenerateTriangle(const MeshData& mesh)
{
  if (mesh.indices.size() % 3u != 0u) {
    return false;
  }

  for (std::size_t i = 0; i < mesh.indices.size(); i += 3u) {
    const std::size_t ia = mesh.indices[i];
    const std::size_t ib = mesh.indices[i + 1u];
    const std::size_t ic = mesh.indices[i + 2u];
    if (ia >= mesh.positions.size() || ib >= mesh.positions.size() || ic >= mesh.positions.size()) {
      continue;
    }
    if (ia == ib || ib == ic || ia == ic) {
      return true;
    }

    const glm::dvec3 ab = glm::dvec3{mesh.positions[ib]} - glm::dvec3{mesh.positions[ia]};
    const glm::dvec3 ac = glm::dvec3{mesh.positions[ic]} - glm::dvec3{mesh.positions[ia]};
    const double edgeProduct = glm::dot(ab, ab) * glm::dot(ac, ac);
    const double areaSquared = glm::dot(glm::cross(ab, ac), glm::cross(ab, ac));
    if (areaSquared <= std::numeric_limits<double>::epsilon() * edgeProduct) {
      return true;
    }
  }
  return false;
}

} // namespace

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

  if (hasDegenerateTriangle(mesh)) {
    errors.push_back(MeshValidationError::DegenerateTriangle);
  }

  return errors;
}

bool isValidMeshData(const MeshData& mesh)
{
  return validateMeshData(mesh).empty();
}

} // namespace rendering::mesh
