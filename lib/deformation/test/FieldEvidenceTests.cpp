#include "deformation/FieldEvidence.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <cstdint>
#include <limits>
#include <vector>

namespace df = deformation;

namespace
{
df::FieldDomain domain(bool volume, double spacingScale = 1.0, unsigned side = 7)
{
  df::DomainGeometry spec;
  spec.dimension = volume ? df::SpatialDimension::Volume : df::SpatialDimension::Plane;
  spec.size = {side, side, volume ? side : 1U};
  spec.spacing = {0.5 * spacingScale, 1.25 * spacingScale, volume ? 2.0 * spacingScale : 1.0};
  spec.origin = {1e6, -2e6, 3e6};
  spec.directions = glm::dmat3(glm::rotate(glm::dmat4(1), 0.4, glm::normalize(glm::dvec3(1, 2, 3))));
  spec.directions[0] *= -1;
  return df::FieldDomain(spec);
}
std::size_t offset(const df::FieldDomain& field, unsigned x, unsigned y, unsigned z)
{
  return (static_cast<std::size_t>(z) * field.size()[1] + y) * field.size()[0] + x;
}
std::vector<glm::vec4> affine(const df::FieldDomain& field, double scale)
{
  std::vector<glm::vec4> values(field.sampleCount());
  for (unsigned z = 0; z < field.size()[2]; ++z)
    for (unsigned y = 0; y < field.size()[1]; ++y)
      for (unsigned x = 0; x < field.size()[0]; ++x)
        values[offset(field, x, y, z)] = glm::vec4(scale * field.indexVectorToPhysical({x, y, z}), 1.0);
  return values;
}
} // namespace

TEST_CASE("Physical field sampling interpolates oblique 2D and 3D maps", "[deformation][quality]")
{
  for (bool volume : {false, true}) {
    const auto field = domain(volume);
    auto values = affine(field, 0.1);
    const glm::dvec3 at(2.25, 3.5, volume ? 1.75 : 0.0);
    const auto sampled = df::sampleDisplacement({field, values}, field.indexToPhysical(at));
    REQUIRE(sampled);
    REQUIRE(glm::length(*sampled - 0.1 * field.indexVectorToPhysical(at)) < 1e-7);
    values[offset(field, 3, 4, volume ? 2 : 0)].w = 0.0f;
    REQUIRE_FALSE(df::sampleDisplacement({field, values}, field.indexToPhysical(at)));
    // A zero-weight invalid neighbor must not invalidate an exact grid-center sample.
    REQUIRE(df::sampleDisplacement({field, values}, field.indexToPhysical({2, 3, volume ? 1.0 : 0.0})));
  }
}

TEST_CASE("Refinement compares both directions at centers and off-grid points", "[deformation][quality]")
{
  for (bool volume : {false, true}) {
    const auto coarse = domain(volume);
    const auto fine = domain(volume, 0.5);
    auto candidate = affine(coarse, 0.1);
    auto refined = affine(fine, 0.1);
    const auto identical =
      df::compareRefinement({{coarse, candidate}, {coarse, candidate}}, {{fine, refined}, {fine, refined}});
    REQUIRE_FALSE(identical.checked()); // Fine grid covers only half of coarse physical extent.
    REQUIRE(identical.unavailable > 0);
    const auto coveringFine = domain(volume, 0.5, 13);
    auto coveringValues = affine(coveringFine, 0.1);
    const auto crossGrid = df::compareRefinement(
      {{coarse, candidate}, {coarse, candidate}},
      {{coveringFine, coveringValues}, {coveringFine, coveringValues}});
    REQUIRE(crossGrid.checked());
    REQUIRE(crossGrid.maxErrorMm < 1e-6);
    const auto sameGrid =
      df::compareRefinement({{coarse, candidate}, {coarse, candidate}}, {{coarse, candidate}, {coarse, candidate}});
    REQUIRE(sameGrid.checked());
    REQUIRE(sameGrid.maxErrorMm == Catch::Approx(0.0));
    auto changed = candidate;
    changed[offset(coarse, 2, 2, volume ? 2 : 0)] += glm::vec4(0.2 * coarse.directions()[0], 0.0);
    const auto perturbed =
      df::compareRefinement({{coarse, candidate}, {coarse, candidate}}, {{coarse, changed}, {coarse, changed}});
    REQUIRE(perturbed.checked());
    REQUIRE(perturbed.maxErrorMm > 0.19);
    REQUIRE_THROWS_AS(
      df::compareRefinement({{coarse, candidate}, {coarse, candidate}}, {{coarse, candidate}, {coarse, candidate}}, 1),
      std::invalid_argument);
  }
}

TEST_CASE("Protected cores include off-grid interior and both directions", "[deformation][quality]")
{
  for (bool volume : {false, true}) {
    const auto field = domain(volume);
    std::vector<glm::vec4> zero(field.sampleCount(), {0, 0, 0, 1});
    const df::ProtectedRegion core{field.indexToPhysical({3.25, 3.25, volume ? 3.25 : 0.0}), 0.2, 1.0};
    const auto exact = df::measureProtectedCores({{field, zero}, {field, zero}}, std::span(&core, 1), 1e-5);
    REQUIRE(exact.checked);
    REQUIRE(exact.intersectingCells > 0);
    REQUIRE(exact.maxErrorMm == 0.0);
    auto moved = zero;
    moved[offset(field, 3, 3, volume ? 3 : 0)] = glm::vec4(0.2 * field.directions()[0], 1.0);
    const auto drift = df::measureProtectedCores({{field, zero}, {field, moved}}, std::span(&core, 1), 1e-5);
    REQUIRE(drift.checked);
    REQUIRE(drift.maxErrorMm > 0.0);
    moved[offset(field, 3, 3, volume ? 3 : 0)].x = std::numeric_limits<float>::quiet_NaN();
    REQUIRE_FALSE(df::measureProtectedCores({{field, zero}, {field, moved}}, std::span(&core, 1), 1e-5).checked);
    REQUIRE_FALSE(df::measureProtectedCores({{field, zero}, {field, zero}}, std::span(&core, 1), 1e-5, 5, 0).checked);
  }
  const auto singleton = domain(true, 1.0, 1);
  const std::vector<glm::vec4> one(singleton.sampleCount(), {0, 0, 0, 1});
  const df::ProtectedRegion point{singleton.indexToPhysical({0, 0, 0}), 0.0, 1.0};
  REQUIRE_FALSE(df::measureProtectedCores({{singleton, one}, {singleton, one}}, std::span(&point, 1), 0.0).checked);
}
