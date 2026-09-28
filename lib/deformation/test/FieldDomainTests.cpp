#include "deformation/FieldDomain.h"

#include "Fixtures.h"
#include "deformation/ContractError.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <glm/geometric.hpp>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <limits>

namespace
{
using namespace deformation;

template<class Operation>
void requireFailure(ContractFailure reason, Operation operation)
{
  try {
    operation();
    FAIL("Expected a typed contract failure");
  }
  catch (const ContractError& error) {
    CHECK(error.reason() == reason);
    CHECK(error.what()[0] != '\0');
  }
}

void checkVector(const glm::dvec3& actual, const glm::dvec3& expected, double tolerance = 1.0e-10)
{
  CHECK(actual.x == Catch::Approx(expected.x).margin(tolerance));
  CHECK(actual.y == Catch::Approx(expected.y).margin(tolerance));
  CHECK(actual.z == Catch::Approx(expected.z).margin(tolerance));
}
} // namespace

TEST_CASE("Domain identity uses sample centers and explicit LPS millimeters", "[deformation][domain]")
{
  DomainGeometry geometry;
  geometry.size = {4, 5, 6};
  const FieldDomain domain{geometry};
  CHECK(domain.dimension() == SpatialDimension::Volume);
  CHECK(domain.convention() == CoordinateConvention::LpsMillimeters);
  CHECK(domain.size() == geometry.size);
  CHECK(domain.sampleCount() == 120);
  CHECK(domain.validExtent().begin == GridSize{0, 0, 0});
  CHECK(domain.validExtent().end == geometry.size);
  checkVector(domain.indexToPhysical({0.0, 0.0, 0.0}), {0.0, 0.0, 0.0});
  checkVector(domain.physicalToIndex({1.25, 2.5, 3.75}), {1.25, 2.5, 3.75});
  CHECK(domain.containsIndex({3.0, 4.0, 5.0}));
  CHECK_FALSE(domain.containsIndex({3.1, 4.0, 5.0}));
  CHECK_FALSE(domain.containsIndex({-0.1, 0.0, 0.0}));
  // Geometry conversion allows extrapolation; it is not a sampling-validity assertion.
  checkVector(domain.indexToPhysical({-2.0, 8.0, 10.0}), {-2.0, 8.0, 10.0});
}

TEST_CASE("Physical conversions preserve anisotropy obliquity and large origins", "[deformation][domain]")
{
  DomainGeometry geometry;
  geometry.size = {20, 30, 40};
  geometry.origin = {1.0e6, -2.0e6, 3.0e6};
  geometry.spacing = {0.2, 1.5, 4.0};
  geometry.directions = deformation::test::obliqueFrame();
  const FieldDomain domain{geometry};
  const glm::dvec3 index{2.25, 3.5, 4.75};
  const glm::dvec3 expected = geometry.origin + geometry.directions * (geometry.spacing * index);
  checkVector(domain.indexToPhysical(index), expected);
  checkVector(domain.physicalToIndex(expected), index, 2.0e-9);
  checkVector(domain.origin(), geometry.origin);
  checkVector(domain.spacing(), geometry.spacing);
  CHECK(domain.directions() == geometry.directions);
  checkVector(domain.physicalVectorToIndex(domain.indexVectorToPhysical(index)), index);
  checkVector(domain.indexVectorToPhysical(index), expected - geometry.origin, 5.0e-10);
  // Construction copies its inputs; later caller edits cannot change the domain.
  geometry.origin = {0.0, 0.0, 0.0};
  CHECK(domain.origin().x == 1.0e6);
}

TEST_CASE("Native 2D has tangent motion and rejects normal coordinates", "[deformation][domain]")
{
  DomainGeometry geometry;
  geometry.dimension = SpatialDimension::Plane;
  geometry.size = {20, 30, 1};
  geometry.origin = {20.0, -10.0, 40.0};
  geometry.spacing = {0.3, 2.0, 1.0};
  geometry.directions = deformation::test::obliqueFrame();
  const FieldDomain domain{geometry};
  const glm::dvec3 index{3.25, 4.5, 0.0};
  const glm::dvec3 point = domain.indexToPhysical(index);
  checkVector(domain.physicalToIndex(point), index);
  const glm::dvec3 vector = domain.indexVectorToPhysical(index);
  CHECK(glm::dot(vector, geometry.directions[2]) == Catch::Approx(0.0).margin(1.0e-12));
  checkVector(domain.physicalVectorToIndex(vector), index);
  requireFailure(ContractFailure::OutsidePlane, [&] { (void)domain.indexToPhysical({1.0, 2.0, 0.01}); });
  requireFailure(ContractFailure::OutsidePlane, [&] {
    (void)domain.physicalToIndex(point + 0.01 * geometry.directions[2]);
  });
  requireFailure(ContractFailure::OutsidePlane, [&] { (void)domain.physicalVectorToIndex(geometry.directions[2]); });
  checkVector(domain.physicalToIndex(point + 0.1 * FieldDomain::planeToleranceMm * geometry.directions[2]), index);
  CHECK_FALSE(domain.containsIndex({1.0, 2.0, 0.01}));

  geometry.dimension = SpatialDimension::Volume;
  const FieldDomain oneSliceVolume{geometry};
  CHECK(oneSliceVolume.dimension() == SpatialDimension::Volume);
  checkVector(oneSliceVolume.physicalToIndex(point + geometry.directions[2]), {index.x, index.y, 1.0});
  CHECK_FALSE(oneSliceVolume.containsIndex({index.x, index.y, 1.0}));
}

TEST_CASE("Reflected axes and small direction rounding are not silently changed", "[deformation][domain]")
{
  DomainGeometry geometry;
  geometry.directions[0][0] = -1.0;
  const FieldDomain reflected{geometry};
  checkVector(reflected.indexToPhysical({2.0, 3.0, 4.0}), {-2.0, 3.0, 4.0});
  checkVector(reflected.physicalToIndex({-2.0, 3.0, 4.0}), {2.0, 3.0, 4.0});
  geometry.directions[0][0] = 1.0 + 1.0e-9;
  const FieldDomain rounded{geometry};
  CHECK(rounded.directions()[0][0] == geometry.directions[0][0]);
  checkVector(rounded.physicalToIndex(rounded.indexToPhysical({50.0, 20.0, -10.0})), {50.0, 20.0, -10.0});
}

TEST_CASE("Valid extents describe centers not half-voxel boundaries", "[deformation][domain]")
{
  DomainGeometry geometry;
  geometry.size = {10, 12, 14};
  geometry.validExtent = IndexExtent{{2, 3, 4}, {8, 9, 10}};
  const FieldDomain domain{geometry};
  CHECK(domain.sampleCount() == 1680);
  CHECK(domain.containsIndex({2.0, 3.0, 4.0}));
  CHECK(domain.containsIndex({7.0, 8.0, 9.0}));
  CHECK(domain.containsIndex({3.5, 4.5, 5.5}));
  CHECK_FALSE(domain.containsIndex({1.9, 3.0, 4.0}));
  CHECK_FALSE(domain.containsIndex({7.5, 8.0, 9.0}));
}

TEST_CASE("Extreme spacing does not overflow or underflow the matrix determinant", "[deformation][domain]")
{
  DomainGeometry geometry;
  for (const double spacing : {1.0e-150, 1.0e150}) {
    geometry.spacing = glm::dvec3{spacing};
    const FieldDomain domain{geometry};
    checkVector(domain.physicalVectorToIndex(domain.indexVectorToPhysical({2.0, 3.0, 4.0})), {2.0, 3.0, 4.0});
  }
  geometry.spacing = {1.0e-150, 1.0, 1.0e150};
  const FieldDomain anisotropic{geometry};
  checkVector(anisotropic.physicalVectorToIndex(anisotropic.indexVectorToPhysical({2.0, 3.0, 4.0})), {2.0, 3.0, 4.0});
}

TEST_CASE("Bad domain geometry produces typed contract errors", "[deformation][domain]")
{
  DomainGeometry geometry;
  SECTION("unknown dimension")
  {
    // Exercise malformed adapter input; the enum has a fixed underlying integer type.
    // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
    geometry.dimension = static_cast<SpatialDimension>(4);
    requireFailure(ContractFailure::InvalidDimension, [&] { (void)FieldDomain{geometry}; });
  }
  SECTION("empty axis")
  {
    geometry.size[1] = 0;
    requireFailure(ContractFailure::EmptyDomain, [&] { (void)FieldDomain{geometry}; });
  }
  SECTION("overflow before allocation")
  {
    geometry.size.fill(std::numeric_limits<std::uint32_t>::max());
    requireFailure(ContractFailure::SampleCountOverflow, [&] { (void)FieldDomain{geometry}; });
  }
  SECTION("nonfinite origin")
  {
    geometry.origin.x = std::numeric_limits<double>::infinity();
    requireFailure(ContractFailure::InvalidOrigin, [&] { (void)FieldDomain{geometry}; });
  }
  SECTION("invalid spacings")
  {
    for (double bad :
         {0.0,
          -1.0,
          std::numeric_limits<double>::quiet_NaN(),
          std::numeric_limits<double>::infinity(),
          std::numeric_limits<double>::denorm_min()})
    {
      geometry.spacing.y = bad;
      requireFailure(ContractFailure::InvalidSpacing, [&] { (void)FieldDomain{geometry}; });
    }
  }
  SECTION("spacing and rounded directions cannot overflow the forward matrix")
  {
    geometry.directions[0][0] = 1.0 + 1.0e-9;
    geometry.spacing.x = std::numeric_limits<double>::max();
    requireFailure(ContractFailure::InvalidSpacing, [&] { (void)FieldDomain{geometry}; });
  }
  SECTION("nonfinite directions")
  {
    geometry.directions[2].x = std::numeric_limits<double>::quiet_NaN();
    requireFailure(ContractFailure::InvalidDirections, [&] { (void)FieldDomain{geometry}; });
  }
  SECTION("sheared directions")
  {
    geometry.directions[1].x = 0.2;
    requireFailure(ContractFailure::InvalidDirections, [&] { (void)FieldDomain{geometry}; });
  }
  SECTION("singular directions")
  {
    geometry.directions[1] = geometry.directions[0];
    requireFailure(ContractFailure::InvalidDirections, [&] { (void)FieldDomain{geometry}; });
  }
  SECTION("planar geometry is explicit")
  {
    geometry.dimension = SpatialDimension::Plane;
    geometry.size[2] = 2;
    requireFailure(ContractFailure::InvalidPlanarGeometry, [&] { (void)FieldDomain{geometry}; });
    geometry.size[2] = 1;
    geometry.spacing.z = 2.0;
    requireFailure(ContractFailure::InvalidPlanarGeometry, [&] { (void)FieldDomain{geometry}; });
  }
  SECTION("extent is inside the grid and nonempty")
  {
    geometry.validExtent = IndexExtent{{0, 0, 0}, {2, 1, 1}};
    requireFailure(ContractFailure::InvalidExtent, [&] { (void)FieldDomain{geometry}; });
    geometry.validExtent = IndexExtent{{1, 0, 0}, {1, 1, 1}};
    requireFailure(ContractFailure::InvalidExtent, [&] { (void)FieldDomain{geometry}; });
  }
}

TEST_CASE("Nonfinite conversion results are rejected without clamping", "[deformation][domain]")
{
  const FieldDomain domain{DomainGeometry{}};
  for (double bad : {std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity()}) {
    const glm::dvec3 value{bad, 0.0, 0.0};
    CHECK_FALSE(domain.containsIndex(value));
    requireFailure(ContractFailure::NonFiniteCoordinate, [&] { (void)domain.indexToPhysical(value); });
    requireFailure(ContractFailure::NonFiniteCoordinate, [&] { (void)domain.physicalToIndex(value); });
    requireFailure(ContractFailure::NonFiniteCoordinate, [&] { (void)domain.indexVectorToPhysical(value); });
    requireFailure(ContractFailure::NonFiniteCoordinate, [&] { (void)domain.physicalVectorToIndex(value); });
  }
  DomainGeometry geometry;
  geometry.spacing.x = std::numeric_limits<double>::max();
  const FieldDomain hugeSpacing{geometry};
  requireFailure(ContractFailure::NonFiniteCoordinate, [&] { (void)hugeSpacing.indexToPhysical({2.0, 0.0, 0.0}); });
  geometry.spacing.x = 1.0;
  geometry.origin.x = std::numeric_limits<double>::max();
  const FieldDomain hugeOrigin{geometry};
  requireFailure(ContractFailure::NonFiniteCoordinate, [&] {
    (void)hugeOrigin.physicalToIndex({-std::numeric_limits<double>::max(), 0.0, 0.0});
  });
}
