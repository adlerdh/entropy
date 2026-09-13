#pragma once

#include "mesh/MeshTypes.h"

#include <expected>
#include <string>

namespace mesh
{

/// Categories of mesh validation and coordinate transformation failures
enum class MeshTransformErrorCode
{
  InvalidGeometry,
  NonFiniteMatrix,
  NonAffineMatrix,
  SingularMatrix,
  PointTransformFailed,
  NonFiniteResult
};

/// Failure details returned by mesh validation and transformation operations
struct MeshTransformError
{
  MeshTransformErrorCode code = MeshTransformErrorCode::InvalidGeometry;
  std::string message;
};

/// Abstract point deformation using nonlinear transformation. Implementations must transform one world-space point at
/// a time and return a descriptive failure.
class IPointTransform
{
public:
  virtual ~IPointTransform() = default;

  /// Transform one point, returning a description when the deformation cannot be evaluated.
  [[nodiscard]] virtual std::expected<glm::dvec3, std::string> transformPoint(const glm::dvec3& point) const = 0;
};

[[nodiscard]] std::expected<glm::dmat4, MeshTransformError> inverseAffine(const glm::dmat4& matrix);
[[nodiscard]] glm::dmat4 rasToLpsMatrix() noexcept;
[[nodiscard]] glm::dvec3 transformPoint(const glm::dmat4& matrix, const glm::dvec3& point) noexcept;
[[nodiscard]] double linearDeterminant(const glm::dmat4& matrix) noexcept;

/// Transform vertices, repair reflected winding, and regenerate area-weighted vertex normals
[[nodiscard]] std::expected<MeshGeometry, MeshTransformError>
transformGeometry(const MeshGeometry& geometry, const glm::dmat4& affine, const IPointTransform* deformation = nullptr);

[[nodiscard]] std::expected<void, MeshTransformError> validateGeometry(const MeshGeometry& geometry);
void regenerateNormals(MeshGeometry& geometry);

} // namespace mesh
