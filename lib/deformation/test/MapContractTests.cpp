#include "deformation/MapPairDescriptor.h"

#include "Fixtures.h"
#include "deformation/ContractError.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <glm/geometric.hpp>
#include <glm/matrix.hpp>

#include <cstddef>
#include <type_traits>

using namespace deformation;

TEST_CASE("Paired descriptors keep distinct forward and inverse sampling domains", "[deformation][maps]")
{
  DomainGeometry source;
  source.size = {4, 5, 6};
  source.spacing = {0.5, 0.5, 2.0};
  DomainGeometry output;
  output.size = {10, 11, 12};
  output.origin = {-2.0, 3.0, 5.0};
  output.directions = deformation::test::obliqueFrame();
  const MapPairDescriptor pair{FieldDomain{source}, FieldDomain{output}, RevisionId{7}, NumericalPolicyVersion{1}};
  CHECK(pair.sourceDomain().size() == source.size);
  CHECK(pair.outputDomain().size() == output.size);
  CHECK(&pair.samplingDomain(MapDirection::Forward) == &pair.sourceDomain());
  CHECK(&pair.samplingDomain(MapDirection::Inverse) == &pair.outputDomain());
  CHECK(pair.revision() == RevisionId{7});
  CHECK(pair.policyVersion() == NumericalPolicyVersion{1});
  STATIC_REQUIRE_FALSE(std::is_convertible_v<RevisionId, NumericalPolicyVersion>);
  STATIC_REQUIRE_FALSE(std::is_default_constructible_v<MapPairDescriptor>);
  STATIC_REQUIRE(std::is_same_v<decltype(pair.sourceDomain()), const FieldDomain&>);
  try {
    // Exercise malformed adapter input; the enum has a fixed underlying integer type.
    // NOLINTNEXTLINE(clang-analyzer-optin.core.EnumCastOutOfRange)
    (void)pair.samplingDomain(static_cast<MapDirection>(99));
    FAIL("Unknown map direction must be rejected");
  }
  catch (const ContractError& error) {
    CHECK(error.reason() == ContractFailure::InvalidMapDirection);
  }
}

TEST_CASE("Paired descriptors reject dimension mismatches and unassigned identities", "[deformation][maps]")
{
  DomainGeometry geometry;
  const FieldDomain volume{geometry};
  geometry.dimension = SpatialDimension::Plane;
  const FieldDomain plane{geometry};
  const auto check = [](
                       const FieldDomain& source,
                       const FieldDomain& output,
                       RevisionId revision,
                       NumericalPolicyVersion version,
                       ContractFailure expected) {
    try {
      (void)MapPairDescriptor{source, output, revision, version};
      FAIL("Expected invalid paired-map metadata");
    }
    catch (const ContractError& error) {
      CHECK(error.reason() == expected);
    }
  };
  check(volume, plane, RevisionId{1}, NumericalPolicyVersion{1}, ContractFailure::IncompatibleDomains);
  check(volume, volume, RevisionId{}, NumericalPolicyVersion{1}, ContractFailure::InvalidRevision);
  check(volume, volume, RevisionId{1}, NumericalPolicyVersion{}, ContractFailure::InvalidPolicyVersion);
  // A total 2D mapping can connect differently embedded planes through its baseline.
  geometry.origin.z = 10.0;
  geometry.directions = deformation::test::obliqueFrame();
  CHECK_NOTHROW(MapPairDescriptor{plane, FieldDomain{geometry}, RevisionId{2}, NumericalPolicyVersion{1}});
}

TEST_CASE("Physical fixtures sample scalar images and paired displacement fields", "[deformation][fixtures]")
{
  DomainGeometry geometry;
  geometry.size = {3, 4, 5};
  geometry.spacing = {0.5, 1.5, 3.0};
  geometry.origin = {100.0, -20.0, 3.0};
  geometry.directions = deformation::test::obliqueFrame();
  const FieldDomain domain{geometry};
  const auto identity = deformation::test::affinePair(glm::dmat3{1.0}, glm::dvec3{0.0});
  const auto identityValues = deformation::test::displacementSamples(domain, identity.forward);
  REQUIRE(identityValues.size() == domain.sampleCount());
  for (const auto& value : identityValues) {
    CHECK(glm::length(value) == Catch::Approx(0.0).margin(1.0e-12));
  }
  const auto translation = deformation::test::affinePair(glm::dmat3{1.0}, glm::dvec3{2.0, -1.0, 3.0});
  const auto forward = deformation::test::displacementSamples(domain, translation.forward);
  const auto inverse = deformation::test::displacementSamples(domain, translation.inverse);
  for (std::size_t i = 0; i < forward.size(); ++i) {
    CHECK(glm::length(forward[i] - glm::dvec3{2.0, -1.0, 3.0}) == Catch::Approx(0.0).margin(1.0e-12));
    CHECK(glm::length(forward[i] + inverse[i]) == Catch::Approx(0.0).margin(1.0e-12));
  }
  const auto scalar = deformation::test::scalarSamples(domain, geometry.origin, 2.0);
  REQUIRE(scalar.size() == domain.sampleCount());
  CHECK(scalar.front() == Catch::Approx(1.0));
  CHECK(scalar[1] > scalar[3]); // x spacing is smaller than y spacing; x is the fastest index.

  geometry.dimension = SpatialDimension::Plane;
  geometry.size[2] = 1;
  geometry.spacing.z = 1.0;
  const FieldDomain plane{geometry};
  CHECK(deformation::test::scalarSamples(plane, geometry.origin, 2.0).size() == 12);
}

TEST_CASE("Composed edits require reverse-order inverse composition", "[deformation][maps]")
{
  const auto baseline = deformation::test::affinePair(glm::dmat3{2.0}, glm::dvec3{0.0});
  const auto edit = deformation::test::affinePair(glm::dmat3{1.0}, glm::dvec3{3.0, 0.0, 0.0});
  const glm::dvec3 point{4.0, 2.0, 1.0};
  const auto forward = deformation::test::compose(edit.forward, baseline.forward);
  const auto inverse = deformation::test::compose(baseline.inverse, edit.inverse);
  CHECK(forward(point).x == Catch::Approx(11.0));
  CHECK(deformation::test::compose(baseline.forward, edit.forward)(point).x == Catch::Approx(14.0));
  CHECK(glm::length(inverse(forward(point)) - point) == Catch::Approx(0.0).margin(1.0e-12));
  CHECK(glm::length(forward(inverse(point)) - point) == Catch::Approx(0.0).margin(1.0e-12));
  CHECK(glm::length(deformation::test::compose(edit.inverse, baseline.inverse)(forward(point)) - point) > 1.0);
}

TEST_CASE("Independent displacement strength scaling destroys inverse consistency", "[deformation][maps]")
{
  const auto pair = deformation::test::affinePair(glm::dmat3{2.0}, glm::dvec3{0.0});
  const auto scaled = [](const deformation::test::PointMap& map) {
    return [map](const glm::dvec3& point) {
      return point + 0.5 * (map(point) - point);
    };
  };
  const glm::dvec3 point{4.0, 0.0, 0.0};
  CHECK(pair.inverse(pair.forward(point)).x == Catch::Approx(4.0));
  CHECK(scaled(pair.inverse)(scaled(pair.forward)(point)).x == Catch::Approx(4.5));
}

TEST_CASE("Valid metadata and inverse consistency alone do not certify an acceptable warp", "[deformation][fixtures]")
{
  DomainGeometry geometry;
  geometry.size = {5, 1, 1};
  geometry.origin = {-2.0, 0.0, 0.0};
  const FieldDomain domain{geometry};
  CHECK_NOTHROW(MapPairDescriptor{domain, domain, RevisionId{1}, NumericalPolicyVersion{1}});
  glm::dmat3 reflected{1.0};
  reflected[0][0] = -1.0;
  const auto invalidOrientation = deformation::test::affinePair(reflected, glm::dvec3{0.0});
  CHECK(glm::determinant(reflected) < 0.0);
  const glm::dvec3 point{1.0, 2.0, 3.0};
  CHECK(glm::length(invalidOrientation.inverse(invalidOrientation.forward(point)) - point) == Catch::Approx(0.0));
  // A true fold: x -> x*x maps two distinct physical locations to the same output.
  const deformation::test::PointMap fold = [](const glm::dvec3& p) {
    return glm::dvec3{p.x * p.x, p.y, p.z};
  };
  CHECK(fold({-1.0, 0.0, 0.0}) == fold({1.0, 0.0, 0.0}));
  const auto malformed = deformation::test::displacementSamples(domain, fold);
  REQUIRE(malformed.size() == domain.sampleCount());
  // The collision is present in stored samples too, not just outside the fixture's domain.
  CHECK(-1.0 + malformed[1].x == Catch::Approx(1.0 + malformed[3].x));
}
