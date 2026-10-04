#include "deformation/QualityPolicy.h"

#include "Fixtures.h"
#include "deformation/ContractError.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <glm/matrix.hpp>

#include <cmath>
#include <limits>

using namespace deformation;

namespace
{
QualityReport sampledIdentity()
{
  QualityReport report;
  const auto jacobian = analyzeJacobian(glm::dmat3{1.0}, SpatialDimension::Volume);
  observeQuality(report.forward, jacobian, {});
  observeQuality(report.inverse, jacobian, {});
  report.protectionChecked = true;
  report.convergenceChecked = true;
  return report;
}
} // namespace

TEST_CASE("Jacobian diagnostics detect directional distortion hidden by determinant", "[deformation][quality]")
{
  glm::dmat3 jacobian{1.0};
  jacobian[0][0] = 100.0;
  jacobian[1][1] = 0.01;
  const auto rotation = deformation::test::obliqueFrame();
  const auto metrics = analyzeJacobian(rotation * jacobian * glm::transpose(rotation), SpatialDimension::Volume);
  REQUIRE(metrics.finite);
  CHECK(metrics.determinant == Catch::Approx(1.0).margin(1.0e-9));
  CHECK(metrics.minSingularValue == Catch::Approx(0.01).margin(1.0e-7));
  CHECK(metrics.maxSingularValue == Catch::Approx(100.0));
  auto report = sampledIdentity();
  observeQuality(report.forward, metrics, {});
  CHECK(assessCandidate(report).reason == QualityReason::Distortion);
  jacobian = glm::dmat3{1.0};
  jacobian[1][0] = 2.0;
  const auto shear = analyzeJacobian(jacobian, SpatialDimension::Volume);
  CHECK(shear.minSingularValue == Catch::Approx(std::sqrt(2.0) - 1.0));
  CHECK(shear.maxSingularValue == Catch::Approx(std::sqrt(2.0) + 1.0));
}

TEST_CASE("Sampled evidence cannot masquerade as verified cells", "[deformation][quality]")
{
  const auto report = sampledIdentity();
  CHECK(assessCandidate(report).decision == CandidateDecision::Refine);
  CHECK(assessCandidate(report).reason == QualityReason::MissingEvidence);
  QualityPolicy sampledPolicy;
  sampledPolicy.requireVerifiedCells = false;
  CHECK(assessCandidate(report, sampledPolicy).decision == CandidateDecision::Accept);
  auto unmeasuredProtection = report;
  unmeasuredProtection.protectionChecked = false;
  CHECK(assessCandidate(unmeasuredProtection, sampledPolicy).reason == QualityReason::MissingEvidence);
  CHECK(assessCandidate({}, sampledPolicy).decision == CandidateDecision::Refine);
}

TEST_CASE(
  "Quality policy rejects folds nonfinite values missing coverage and protected motion",
  "[deformation][quality]")
{
  auto report = sampledIdentity();
  SECTION("fold")
  {
    report.inverse.minDeterminant = -0.1;
    CHECK(assessCandidate(report).reason == QualityReason::Folding);
  }
  SECTION("nonfinite")
  {
    observeQuality(report.forward, {}, {});
    CHECK(assessCandidate(report).reason == QualityReason::NonFinite);
  }
  SECTION("coverage")
  {
    observeQuality(report.inverse, {}, {}, false);
    CHECK(assessCandidate(report).reason == QualityReason::Coverage);
  }
  SECTION("protection")
  {
    report.maxProtectionErrorMm = 0.1;
    CHECK(assessCandidate(report).reason == QualityReason::Protection);
  }
  SECTION("invalid policy")
  {
    QualityPolicy policy;
    policy.minDeterminant = 0.0;
    CHECK_THROWS_AS(assessCandidate(report, policy), ContractError);
    policy.minDeterminant = 0.1;
    policy.maxResidualMm = std::numeric_limits<double>::quiet_NaN();
    CHECK_THROWS_AS(assessCandidate(report, policy), ContractError);
  }
}

TEST_CASE("Both physical and voxel residual thresholds apply", "[deformation][quality]")
{
  auto report = sampledIdentity();
  report.forward.maxResidualMm = 0.01;
  report.forward.maxResidualVoxels = 0.1;
  CHECK(assessCandidate(report).reason == QualityReason::InverseConsistency);
  report.forward.maxResidualVoxels = 0.0;
  report.inverse.maxResidualMm = 0.2;
  CHECK(assessCandidate(report).reason == QualityReason::InverseConsistency);
  report.inverse.maxResidualMm = 0.0;
  report.cellsVerified = true;
  report.maxConvergenceErrorMm = 0.1;
  CHECK(assessCandidate(report).reason == QualityReason::Convergence);
}

TEST_CASE("A soft retry must not conceal hard failures in the opposite direction", "[deformation][quality]")
{
  auto report = sampledIdentity();
  report.forward.maxResidualMm = 10.0;
  report.inverse.minDeterminant = -1.0;
  CHECK(assessCandidate(report).decision == CandidateDecision::Reject);
  CHECK(assessCandidate(report).reason == QualityReason::Folding);
  report.forward = {};
  CHECK(assessCandidate(report).decision == CandidateDecision::Reject);
}
