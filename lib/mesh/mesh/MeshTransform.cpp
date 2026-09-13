#include "mesh/MeshTransform.h"

#include <glm/geometric.hpp>
#include <glm/matrix.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace mesh
{
namespace
{
constexpr double sk_affineTolerance = 1.0e-12;
constexpr double sk_singularTolerance = 1.0e-15;

bool isFinite(const glm::vec3& value)
{
  return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

bool isFinite(const glm::dmat4& matrix)
{
  for (glm::length_t column = 0; column < 4; ++column) {
    for (glm::length_t row = 0; row < 4; ++row) {
      if (!std::isfinite(matrix[column][row])) return false;
    }
  }
  return true;
}

bool isAffine(const glm::dmat4& matrix)
{
  return std::abs(matrix[0][3]) <= sk_affineTolerance && std::abs(matrix[1][3]) <= sk_affineTolerance &&
         std::abs(matrix[2][3]) <= sk_affineTolerance && std::abs(matrix[3][3] - 1.0) <= sk_affineTolerance;
}
} // namespace

std::expected<glm::dmat4, MeshTransformError> inverseAffine(const glm::dmat4& matrix)
{
  if (!isFinite(matrix)) {
    return std::unexpected(
      MeshTransformError{MeshTransformErrorCode::NonFiniteMatrix, "Mesh affine transform is non-finite"});
  }

  if (!isAffine(matrix)) {
    return std::unexpected(MeshTransformError{MeshTransformErrorCode::NonAffineMatrix, "Mesh transform is not affine"});
  }

  const double determinant = linearDeterminant(matrix);
  if (!std::isfinite(determinant) || std::abs(determinant) < sk_singularTolerance) {
    return std::unexpected(
      MeshTransformError{MeshTransformErrorCode::SingularMatrix, "Mesh affine transform is singular"});
  }
  return glm::inverse(matrix);
}

glm::dmat4 rasToLpsMatrix() noexcept
{
  glm::dmat4 matrix{1.0};
  matrix[0][0] = -1.0;
  matrix[1][1] = -1.0;
  return matrix;
}

glm::dvec3 transformPoint(const glm::dmat4& matrix, const glm::dvec3& point) noexcept
{
  const glm::dvec4 transformed = matrix * glm::dvec4{point, 1.0};
  if (
    std::isfinite(transformed.w) && std::abs(transformed.w) > std::numeric_limits<double>::epsilon() &&
    transformed.w != 1.0)
  {
    return glm::dvec3{transformed} / transformed.w;
  }
  return glm::dvec3{transformed};
}

double linearDeterminant(const glm::dmat4& matrix) noexcept
{
  return glm::determinant(glm::dmat3{matrix});
}

std::expected<void, MeshTransformError> validateGeometry(const MeshGeometry& geometry)
{
  const auto invalidGeometry = [](std::string message) {
    return std::unexpected(MeshTransformError{MeshTransformErrorCode::InvalidGeometry, std::move(message)});
  };

  if (geometry.positions.empty()) {
    return invalidGeometry("Mesh contains no vertices");
  }

  if (geometry.triangleIndices.empty() || geometry.triangleIndices.size() % 3u != 0u) {
    return invalidGeometry("Mesh does not contain a valid triangle index buffer");
  }

  if (!geometry.normals.empty() && geometry.normals.size() != geometry.positions.size()) {
    return invalidGeometry("Mesh normal count does not match its vertex count");
  }

  if (!std::ranges::all_of(geometry.positions, [](const glm::vec3& value) { return isFinite(value); })) {
    return invalidGeometry("Mesh contains non-finite vertices");
  }

  if (!std::ranges::all_of(geometry.normals, [](const glm::vec3& value) { return isFinite(value); })) {
    return invalidGeometry("Mesh contains non-finite normals");
  }

  if (std::ranges::any_of(geometry.triangleIndices, [&geometry](uint32_t index) {
        return static_cast<std::size_t>(index) >= geometry.positions.size();
      }))
  {
    return invalidGeometry("Mesh contains an out-of-range triangle index");
  }
  return {};
}

void regenerateNormals(MeshGeometry& geometry)
{
  std::vector<glm::dvec3> accumulated(geometry.positions.size(), glm::dvec3{0.0});
  for (std::size_t index = 0; index + 2u < geometry.triangleIndices.size(); index += 3u) {
    const uint32_t ia = geometry.triangleIndices[index];
    const uint32_t ib = geometry.triangleIndices[index + 1u];
    const uint32_t ic = geometry.triangleIndices[index + 2u];

    const glm::dvec3 normal = glm::cross(
      glm::dvec3{geometry.positions[ib]} - glm::dvec3{geometry.positions[ia]},
      glm::dvec3{geometry.positions[ic]} - glm::dvec3{geometry.positions[ia]});
    accumulated[ia] += normal;
    accumulated[ib] += normal;
    accumulated[ic] += normal;
  }

  geometry.normals.resize(geometry.positions.size());
  for (std::size_t index = 0; index < accumulated.size(); ++index) {
    const double length = glm::length(accumulated[index]);
    geometry.normals[index] =
      length > std::numeric_limits<double>::epsilon() ? glm::vec3{accumulated[index] / length} : glm::vec3{0.0f};
  }
}

std::expected<MeshGeometry, MeshTransformError>
transformGeometry(const MeshGeometry& geometry, const glm::dmat4& affine, const IPointTransform* deformation)
{
  if (auto valid = validateGeometry(geometry); !valid) {
    return std::unexpected(valid.error());
  }

  if (!isFinite(affine)) {
    return std::unexpected(
      MeshTransformError{MeshTransformErrorCode::NonFiniteMatrix, "Mesh transform must be finite"});
  }

  if (!isAffine(affine)) {
    return std::unexpected(
      MeshTransformError{MeshTransformErrorCode::NonAffineMatrix, "Mesh transform must be affine"});
  }

  const double determinant = linearDeterminant(affine);
  if (!std::isfinite(determinant) || std::abs(determinant) < sk_singularTolerance) {
    return std::unexpected(
      MeshTransformError{MeshTransformErrorCode::SingularMatrix, "Mesh affine transform is singular"});
  }

  MeshGeometry transformed = geometry;
  for (glm::vec3& position : transformed.positions) {
    glm::dvec3 point = transformPoint(affine, glm::dvec3{position});

    if (deformation) {
      auto deformed = deformation->transformPoint(point);
      if (!deformed) {
        return std::unexpected(
          MeshTransformError{MeshTransformErrorCode::PointTransformFailed, std::move(deformed.error())});
      }
      point = *deformed;
    }

    if (!std::isfinite(point.x) || !std::isfinite(point.y) || !std::isfinite(point.z)) {
      return std::unexpected(MeshTransformError{
        MeshTransformErrorCode::NonFiniteResult,
        "Mesh transformation produced a non-finite vertex"});
    }
    position = glm::vec3{point};
  }

  if (determinant < 0.0) {
    for (std::size_t index = 0; index < transformed.triangleIndices.size(); index += 3u) {
      std::swap(transformed.triangleIndices[index + 1u], transformed.triangleIndices[index + 2u]);
    }
  }

  regenerateNormals(transformed);
  return transformed;
}

} // namespace mesh
