#include "deformation/QualityPolicy.h"

#include "deformation/ContractError.h"

#include <glm/matrix.hpp>

#include <algorithm>
#include <cmath>
#include <limits>

namespace deformation
{
namespace
{
double norm(const glm::dvec3& v)
{
  return std::hypot(v.x, v.y, v.z);
}
bool nonnegative(double value)
{
  return std::isfinite(value) && value >= 0.0;
}
} // namespace

JacobianMetrics analyzeJacobian(const glm::dmat3& jacobian, SpatialDimension dimension)
{
  if (dimension != SpatialDimension::Plane && dimension != SpatialDimension::Volume) {
    throw ContractError(ContractFailure::InvalidDimension, "Jacobian analysis requires native 2D or 3D");
  }
  const int n = dimension == SpatialDimension::Plane ? 2 : 3;
  double scale = 0.0;
  for (int c = 0; c < 3; ++c) {
    for (int r = 0; r < 3; ++r) {
      if (!std::isfinite(jacobian[c][r])) return {};
      if (c < n && r < n) scale = std::max(scale, std::abs(jacobian[c][r]));
    }
  }
  const double determinant =
    n == 2 ? jacobian[0][0] * jacobian[1][1] - jacobian[1][0] * jacobian[0][1] : glm::determinant(jacobian);
  if (!std::isfinite(determinant)) return {};
  if (scale == 0.0) return {determinant, 0.0, 0.0, true};
  glm::dmat3 normalized{0.0};
  for (int c = 0; c < n; ++c)
    for (int r = 0; r < n; ++r)
      normalized[c][r] = jacobian[c][r] / scale;
  glm::dmat3 gram = glm::transpose(normalized) * normalized;
  // Symmetric Jacobi rotations; the tiny fixed matrix needs no external solver.
  for (int sweep = 0; sweep < 20; ++sweep) {
    for (int p = 0; p < n; ++p) {
      for (int q = p + 1; q < n; ++q) {
        if (std::abs(gram[q][p]) < 1.0e-16) continue;
        const double angle = 0.5 * std::atan2(2.0 * gram[q][p], gram[q][q] - gram[p][p]);
        glm::dmat3 rotation{1.0};
        rotation[p][p] = std::cos(angle);
        rotation[q][q] = std::cos(angle);
        rotation[q][p] = std::sin(angle);
        rotation[p][q] = -std::sin(angle);
        gram = glm::transpose(rotation) * gram * rotation;
      }
    }
  }
  double minimum = std::numeric_limits<double>::infinity();
  double maximum = 0.0;
  for (int axis = 0; axis < n; ++axis) {
    const double value = scale * std::sqrt(std::max(0.0, gram[axis][axis]));
    minimum = std::min(minimum, value);
    maximum = std::max(maximum, value);
  }
  return {determinant, minimum, maximum, std::isfinite(minimum) && std::isfinite(maximum)};
}

InverseResidual inverseResidual(const glm::dvec3& original, const glm::dvec3& recovered, const FieldDomain& domain)
{
  (void)domain.physicalToIndex(original);
  (void)domain.physicalToIndex(recovered);
  const auto displacement = recovered - original;
  return {norm(displacement), norm(domain.physicalVectorToIndex(displacement))};
}

void observeQuality(DirectionQuality& quality, const JacobianMetrics& jacobian, InverseResidual residual, bool covered)
{
  if (quality.requested == std::numeric_limits<std::size_t>::max()) {
    throw ContractError(ContractFailure::SampleCountOverflow, "Quality sample count overflow");
  }
  ++quality.requested;
  if (!covered) {
    ++quality.outside;
    return;
  }
  ++quality.evaluated;
  quality.finite = quality.finite && jacobian.finite && std::isfinite(jacobian.determinant) &&
                   nonnegative(jacobian.minSingularValue) && nonnegative(jacobian.maxSingularValue) &&
                   jacobian.minSingularValue <= jacobian.maxSingularValue && nonnegative(residual.millimeters) &&
                   nonnegative(residual.voxels);
  quality.minDeterminant = std::min(quality.minDeterminant, jacobian.determinant);
  quality.maxDeterminant = std::max(quality.maxDeterminant, jacobian.determinant);
  quality.minSingularValue = std::min(quality.minSingularValue, jacobian.minSingularValue);
  quality.maxSingularValue = std::max(quality.maxSingularValue, jacobian.maxSingularValue);
  quality.maxResidualMm = std::max(quality.maxResidualMm, residual.millimeters);
  quality.maxResidualVoxels = std::max(quality.maxResidualVoxels, residual.voxels);
}

CandidateAssessment assessCandidate(const QualityReport& report, const QualityPolicy& policy)
{
  if (
    policy.version.value == 0 || !nonnegative(policy.minDeterminant) || policy.minDeterminant == 0.0 ||
    !nonnegative(policy.maxDeterminant) || policy.maxDeterminant < policy.minDeterminant ||
    !nonnegative(policy.minSingularValue) || policy.minSingularValue == 0.0 || !nonnegative(policy.maxSingularValue) ||
    policy.maxSingularValue < policy.minSingularValue || !nonnegative(policy.maxResidualMm) ||
    !nonnegative(policy.maxResidualVoxels) || !nonnegative(policy.maxProtectionErrorMm) ||
    !nonnegative(policy.maxConvergenceErrorMm))
  {
    throw ContractError(
      ContractFailure::InvalidQualityPolicy,
      "Quality limits must be finite, ordered, and forbid folding");
  }
  bool missingEvidence = false;
  bool inverseInconsistent = false;
  for (const auto* q : {&report.forward, &report.inverse}) {
    if (!q->finite) return {CandidateDecision::Reject, QualityReason::NonFinite};
    if (q->requested == 0 || q->evaluated > q->requested || q->outside != q->requested - q->evaluated) {
      missingEvidence = true;
      continue;
    }
    if (q->outside != 0) return {CandidateDecision::Reject, QualityReason::Coverage};
    if (
      !std::isfinite(q->minDeterminant) || !std::isfinite(q->maxDeterminant) || !nonnegative(q->minSingularValue) ||
      !nonnegative(q->maxSingularValue) || !nonnegative(q->maxResidualMm) || !nonnegative(q->maxResidualVoxels) ||
      q->minDeterminant > q->maxDeterminant || q->minSingularValue > q->maxSingularValue)
    {
      return {CandidateDecision::Reject, QualityReason::NonFinite};
    }
    if (q->minDeterminant <= 0.0) return {CandidateDecision::Reject, QualityReason::Folding};
    if (
      q->minDeterminant < policy.minDeterminant || q->maxDeterminant > policy.maxDeterminant ||
      q->minSingularValue < policy.minSingularValue || q->maxSingularValue > policy.maxSingularValue)
    {
      return {CandidateDecision::Reject, QualityReason::Distortion};
    }
    if (q->maxResidualMm > policy.maxResidualMm || q->maxResidualVoxels > policy.maxResidualVoxels) {
      inverseInconsistent = true;
    }
  }
  if (!nonnegative(report.maxProtectionErrorMm) || !nonnegative(report.maxConvergenceErrorMm)) {
    return {CandidateDecision::Reject, QualityReason::NonFinite};
  }
  if (report.maxProtectionErrorMm > policy.maxProtectionErrorMm)
    return {CandidateDecision::Reject, QualityReason::Protection};
  // A soft retry in one direction must not hide a fold or other hard failure in the other.
  if (inverseInconsistent) return {CandidateDecision::Refine, QualityReason::InverseConsistency};
  if (missingEvidence || !report.convergenceChecked || (policy.requireVerifiedCells && !report.cellsVerified)) {
    return {CandidateDecision::Refine, QualityReason::MissingEvidence};
  }
  if (report.maxConvergenceErrorMm > policy.maxConvergenceErrorMm)
    return {CandidateDecision::Refine, QualityReason::Convergence};
  return {CandidateDecision::Accept, QualityReason::Passed};
}
} // namespace deformation
