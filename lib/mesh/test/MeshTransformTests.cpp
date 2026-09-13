#include "mesh/MeshTransform.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <limits>

using namespace mesh;

namespace
{
MeshGeometry triangle()
{
  MeshGeometry geometry;
  geometry.positions = {{0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}};
  geometry.triangleIndices = {0u, 1u, 2u};
  regenerateNormals(geometry);
  return geometry;
}

class OffsetTransform final : public IPointTransform
{
public:
  [[nodiscard]] std::expected<glm::dvec3, std::string> transformPoint(const glm::dvec3& point) const override
  {
    return glm::dvec3{point.x, point.y, point.z + point.x * point.y + 2.0};
  }
};

class FailingTransform final : public IPointTransform
{
public:
  [[nodiscard]] std::expected<glm::dvec3, std::string> transformPoint(const glm::dvec3&) const override
  {
    return std::unexpected("Deformation is outside its valid domain");
  }
};
} // namespace

TEST_CASE("Affine inverse round trips points")
{
  glm::dmat4 matrix{1.0};
  matrix[0][0] = 2.0;
  matrix[1][0] = 0.5;
  matrix[3][0] = 4.0;
  matrix[1][1] = 3.0;
  matrix[3][1] = -2.0;
  matrix[2][2] = 4.0;
  matrix[3][2] = 7.0;
  const auto inverse = inverseAffine(matrix);
  REQUIRE(inverse);
  const glm::dvec3 point{1.0, 2.0, 3.0};
  const glm::dvec3 roundTrip = transformPoint(*inverse, transformPoint(matrix, point));
  CHECK(roundTrip.x == Catch::Approx(point.x));
  CHECK(roundTrip.y == Catch::Approx(point.y));
  CHECK(roundTrip.z == Catch::Approx(point.z));
}

TEST_CASE("Reflected mesh transforms repair winding and normals")
{
  glm::dmat4 reflection{1.0};
  reflection[0][0] = -1.0;
  const auto transformed = transformGeometry(triangle(), reflection);
  REQUIRE(transformed);
  CHECK(transformed->triangleIndices == std::vector<uint32_t>{0u, 2u, 1u});
  CHECK(transformed->normals[0].z == Catch::Approx(1.0f));
}

TEST_CASE("Nonlinear mesh transforms regenerate normals after deforming vertices")
{
  OffsetTransform deformation;
  const auto transformed = transformGeometry(triangle(), glm::dmat4{1.0}, &deformation);
  REQUIRE(transformed);
  CHECK(transformed->positions[0].z == Catch::Approx(2.0f));
  CHECK(transformed->normals.size() == transformed->positions.size());
}

TEST_CASE("RAS to LPS flips the first two axes without reflecting triangle winding")
{
  const glm::dmat4 conversion = rasToLpsMatrix();
  CHECK(linearDeterminant(conversion) == Catch::Approx(1.0));
  const glm::dvec3 converted = transformPoint(conversion, {1.0, 2.0, 3.0});
  CHECK(converted.x == Catch::Approx(-1.0));
  CHECK(converted.y == Catch::Approx(-2.0));
  CHECK(converted.z == Catch::Approx(3.0));
}

TEST_CASE("Mesh transformation rejects projective and non-finite matrices")
{
  glm::dmat4 projective{1.0};
  projective[0][3] = 0.1;
  auto result = transformGeometry(triangle(), projective);
  REQUIRE_FALSE(result);
  CHECK(result.error().code == MeshTransformErrorCode::NonAffineMatrix);

  glm::dmat4 nonFinite{1.0};
  nonFinite[3][0] = std::numeric_limits<double>::infinity();
  result = transformGeometry(triangle(), nonFinite);
  REQUIRE_FALSE(result);
  CHECK(result.error().code == MeshTransformErrorCode::NonFiniteMatrix);

  glm::dmat4 singular{1.0};
  singular[2][2] = 0.0;
  result = transformGeometry(triangle(), singular);
  REQUIRE_FALSE(result);
  CHECK(result.error().code == MeshTransformErrorCode::SingularMatrix);
}

TEST_CASE("Mesh transformation reports deformation failures without throwing")
{
  FailingTransform deformation;
  const auto result = transformGeometry(triangle(), glm::dmat4{1.0}, &deformation);
  REQUIRE_FALSE(result);
  CHECK(result.error().code == MeshTransformErrorCode::PointTransformFailed);
  CHECK(result.error().message == "Deformation is outside its valid domain");
}
