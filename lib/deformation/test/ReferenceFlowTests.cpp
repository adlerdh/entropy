#include "reference/ReferenceFlow.h"

#include "Fixtures.h"
#include "deformation/BrushStep.h"
#include "deformation/ContractError.h"
#include "deformation/QualityPolicy.h"
#include "deformation/StrokeSampling.h"
#include "deformation/VelocityLattice.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <glm/geometric.hpp>

#include <array>
#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <limits>
#include <vector>

using namespace deformation;

TEST_CASE("Double precision RK4 converges to analytic stationary flows", "[deformation][reference]")
{
  const reference::VectorFunction radial = [](const glm::dvec3& p) {
    return 0.8 * p;
  };
  const glm::dvec3 p{1.0, 2.0, 3.0};
  const glm::dvec3 exact = std::exp(0.8) * p;
  const double coarse = glm::length(reference::integrate(radial, p, 2) - exact);
  const double fine = glm::length(reference::integrate(radial, p, 4) - exact);
  CHECK(fine < coarse / 8.0);
  const auto estimate = reference::integrateConverged(radial, p);
  REQUIRE(estimate.converged);
  CHECK(glm::length(estimate.pointMm - exact) < 1.0e-9);
  const auto inverse = reference::integrateConverged(radial, estimate.pointMm, {}, MapDirection::Inverse);
  REQUIRE(inverse.converged);
  CHECK(glm::length(inverse.pointMm - p) < 1.0e-9);
  reference::IntegrationOptions insufficient;
  insufficient.initialSteps = 1;
  insufficient.maxSteps = 2;
  insufficient.toleranceMm = 1.0e-14;
  CHECK_FALSE(reference::integrateConverged(radial, p, insufficient).converged);
}

TEST_CASE("Compact twirl has an analytic circular orbit and accurate inverse in oblique 2D", "[deformation][reference]")
{
  BrushDefinition d;
  d.dimension = SpatialDimension::Plane;
  d.directions = deformation::test::obliqueFrame();
  d.centerMm = {20.0, -10.0, 5.0};
  d.radiusMm = 8.0;
  d.motion = TwirlMotion{d.directions[2], 0.6};
  const BrushStep brush{d};
  const VelocityLattice lattice{brush, 1.1};
  const glm::dvec3 p = d.centerMm + 2.0 * d.directions[0];
  const double angle = 0.6 * brush.envelope(p);
  const glm::dvec3 exact = d.centerMm + 2.0 * (std::cos(angle) * d.directions[0] + std::sin(angle) * d.directions[1]);
  const reference::VectorFunction velocity = [&](const glm::dvec3& point) {
    return lattice.velocity(point);
  };
  const auto forward = reference::integrateConverged(velocity, p);
  REQUIRE(forward.converged);
  CHECK(glm::length(forward.pointMm - exact) < 1.0e-8);
  const auto inverse = reference::integrateConverged(velocity, forward.pointMm, {}, MapDirection::Inverse);
  REQUIRE(inverse.converged);
  CHECK(glm::length(inverse.pointMm - p) < 1.0e-8);
}

TEST_CASE("Brush flows preserve protected cores and reverse chronological composition", "[deformation][reference]")
{
  BrushDefinition d;
  d.motion = PushMotion{{0.3, 0.0, 0.0}};
  const BrushStep push{d};
  d.motion = RadialMotion{-0.1};
  const BrushStep deflate{d};
  const std::array<reference::VectorFunction, 2> sequence{
    [&](const glm::dvec3& p) { return push.velocity(p); },
    [&](const glm::dvec3& p) {
      return deflate.velocity(p);
    }};
  const glm::dvec3 p{1.0, 1.0, 1.0};
  const auto moved = reference::composeFlows(sequence, p, 64);
  CHECK(glm::length(reference::composeFlows(sequence, moved, 64, MapDirection::Inverse) - p) < 1.0e-10);
  d.protection.push_back({p, 0.5, 1.0});
  const BrushStep protectedStep{d};
  CHECK(reference::integrate([&](const glm::dvec3& point) { return protectedStep.velocity(point); }, p, 16) == p);
}

TEST_CASE(
  "Physical Jacobians and inverse residuals respect native dimensionality and spacing",
  "[deformation][reference]")
{
  for (const auto dimension : {SpatialDimension::Plane, SpatialDimension::Volume}) {
    DomainGeometry geometry;
    geometry.dimension = dimension;
    geometry.directions = deformation::test::obliqueFrame();
    geometry.spacing = {0.2, 2.0, dimension == SpatialDimension::Plane ? 1.0 : 4.0};
    const FieldDomain domain{geometry};
    const reference::VectorFunction map = [](const glm::dvec3& p) {
      return 1.2 * p;
    };
    const auto jacobian = reference::physicalJacobian(map, glm::dvec3{0.0}, domain, 1.0e-3);
    const auto metrics = analyzeJacobian(jacobian, dimension);
    CHECK(metrics.determinant == Catch::Approx(dimension == SpatialDimension::Plane ? 1.44 : 1.728));
    CHECK(metrics.minSingularValue == Catch::Approx(1.2));
    const auto error = inverseResidual(glm::dvec3{0.0}, 0.1 * geometry.directions[0], domain);
    CHECK(error.millimeters == Catch::Approx(0.1));
    CHECK(error.voxels == Catch::Approx(0.5));
  }
}

TEST_CASE("Reference integration refuses non-finite motion and invalid budgets", "[deformation][reference]")
{
  const reference::VectorFunction invalid = [](const glm::dvec3&) {
    return glm::dvec3{std::numeric_limits<double>::quiet_NaN()};
  };
  CHECK_THROWS_AS(reference::integrate(invalid, glm::dvec3{0.0}, 4), ContractError);
  CHECK_THROWS_AS(reference::integrate({}, glm::dvec3{0.0}, 4), ContractError);
  CHECK_THROWS_AS(reference::integrate(invalid, glm::dvec3{0.0}, 0), ContractError);
}

TEST_CASE("Refining canonical drag steps stabilizes the composed deformation", "[deformation][reference]")
{
  const std::array<StrokeSample, 2> samples{{{{0.0, 0.0, 0.0}, 0.0}, {{2.0, 0.0, 0.0}, 0.4}}};
  const auto endpoint = [&](double spacing) {
    StrokeSamplingOptions options;
    options.maxDistanceMm = spacing;
    options.maxActiveSeconds = 1.0;
    glm::dvec3 point{0.5, 1.0, 0.0};
    for (const auto& segment : resampleStroke(samples, options)) {
      BrushDefinition d;
      d.radiusMm = 5.0;
      d.centerMm = 0.5 * (segment.beginMm + segment.endMm);
      d.motion = PushMotion{segment.endMm - segment.beginMm};
      const BrushStep brush{d};
      point = reference::integrate([&](const glm::dvec3& p) { return brush.velocity(p); }, point, 16);
    }
    return point;
  };
  const auto reference = endpoint(0.025);
  CHECK(glm::length(endpoint(0.1) - reference) < glm::length(endpoint(0.4) - reference));
}

TEST_CASE("Composed brush pairs pass sampled physical quality checks in both dimensions", "[deformation][reference]")
{
  for (const auto dimension : {SpatialDimension::Plane, SpatialDimension::Volume}) {
    DomainGeometry geometry;
    geometry.dimension = dimension;
    geometry.directions = deformation::test::obliqueFrame();
    geometry.spacing = {0.5, 2.0, dimension == SpatialDimension::Plane ? 1.0 : 3.0};
    const FieldDomain domain{geometry};
    BrushDefinition d;
    d.dimension = dimension;
    d.directions = geometry.directions;
    d.motion = PushMotion{0.2 * d.directions[0]};
    const BrushStep push{d};
    d.motion = RadialMotion{-0.05};
    const BrushStep radial{d};
    const std::array<reference::VectorFunction, 2> steps{
      [&](const glm::dvec3& p) { return push.velocity(p); },
      [&](const glm::dvec3& p) {
        return radial.velocity(p);
      }};
    const reference::VectorFunction forward = [&](const glm::dvec3& p) {
      return reference::composeFlows(steps, p, 32);
    };
    const reference::VectorFunction inverse = [&](const glm::dvec3& p) {
      return reference::composeFlows(steps, p, 32, MapDirection::Inverse);
    };
    QualityReport report;
    for (const double x : {-2.0, -0.2, 0.7, 2.0}) {
      const auto p = domain.indexToPhysical({x, 0.3, dimension == SpatialDimension::Plane ? 0.0 : 0.5});
      observeQuality(
        report.forward,
        analyzeJacobian(reference::physicalJacobian(forward, p, domain, 1.0e-3), dimension),
        inverseResidual(p, inverse(forward(p)), domain));
      observeQuality(
        report.inverse,
        analyzeJacobian(reference::physicalJacobian(inverse, p, domain, 1.0e-3), dimension),
        inverseResidual(p, forward(inverse(p)), domain));
      report.maxConvergenceErrorMm =
        std::max(report.maxConvergenceErrorMm, glm::length(forward(p) - reference::composeFlows(steps, p, 64)));
      report.maxConvergenceErrorMm = std::max(
        report.maxConvergenceErrorMm,
        glm::length(inverse(p) - reference::composeFlows(steps, p, 64, MapDirection::Inverse)));
    }
    report.convergenceChecked = true;
    QualityPolicy policy;
    policy.requireVerifiedCells = false;
    CHECK(assessCandidate(report, policy).decision == CandidateDecision::Accept);
    CHECK(report.forward.maxResidualMm < 1.0e-9);
    CHECK(report.inverse.maxResidualMm < 1.0e-9);
    CHECK(assessCandidate(report).decision == CandidateDecision::Refine);
  }
}
