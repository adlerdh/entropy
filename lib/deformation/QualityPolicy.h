#pragma once

#include "deformation/FieldDomain.h"
#include "deformation/Types.h"

#include <glm/mat3x3.hpp>
#include <glm/vec3.hpp>

#include <cstddef>
#include <limits>

namespace deformation
{
/** @brief Local distortion in intrinsic physical coordinates; a determinant alone cannot measure directional collapse.
 */
struct JacobianMetrics
{
  double determinant = 0.0;      //!< Volume ratio in 3D, area ratio in native 2D; nonpositive is unacceptable.
  double minSingularValue = 0.0; //!< Minimum physical stretch.
  double maxSingularValue = 0.0; //!< Maximum physical stretch.
  bool finite = false;           //!< False if the supplied matrix or calculation is non-finite.
};

/**
 * @brief Analyze a physical Jacobian; native 2D uses the upper-left 2-by-2 intrinsic block.
 * @details Inputs are derivatives with respect to orthonormal physical axes, not
 * raw voxel derivatives. All matrix entries must be finite. Invalid dimension throws ContractError.
 */
[[nodiscard]] JacobianMetrics analyzeJacobian(const glm::dmat3& jacobian, SpatialDimension dimension);

/** @brief Error of a round trip, measured in the domain where that round trip began. */
struct InverseResidual
{
  double millimeters = 0.0; //!< Euclidean physical distance.
  double voxels = 0.0;      //!< Euclidean norm after applying this domain's inverse directions and spacing.
};

/** @brief Measure recovered - original; non-finite or off-plane coordinates throw ContractError. */
[[nodiscard]] InverseResidual
inverseResidual(const glm::dvec3& original, const glm::dvec3& recovered, const FieldDomain& domain);

/** @brief Reduced observations for one map direction; empty data cannot pass acceptance. */
struct DirectionQuality
{
  std::size_t requested = 0; //!< Number of requested samples, including unavailable coverage.
  std::size_t evaluated = 0; //!< Number of available sample pairs.
  std::size_t outside = 0;   //!< Missing coverage, never silently omitted from requested.
  bool finite = true;        //!< All supplied diagnostic values were finite and validly signed residuals.
  double minDeterminant = std::numeric_limits<double>::infinity();   //!< Minimum observed Jacobian determinant.
  double maxDeterminant = -std::numeric_limits<double>::infinity();  //!< Maximum observed Jacobian determinant.
  double minSingularValue = std::numeric_limits<double>::infinity(); //!< Minimum observed physical stretch.
  double maxSingularValue = 0.0;                                     //!< Maximum observed physical stretch.
  double maxResidualMm = 0.0;                                        //!< Maximum physical inverse error.
  double maxResidualVoxels = 0.0;                                    //!< Maximum voxel-normalized inverse error.
};

/** @brief Accumulate one available sample or count missing coverage; does not assert cell verification. */
void observeQuality(
  DirectionQuality& quality,
  const JacobianMetrics& jacobian,
  InverseResidual residual,
  bool covered = true);

/** @brief Candidate evidence produced by a numerical backend, independently of rendering. */
struct QualityReport
{
  DirectionQuality forward;           //!< Source-domain round trips and forward Jacobians.
  DirectionQuality inverse;           //!< Output-domain round trips and inverse Jacobians.
  double maxProtectionErrorMm = 0.0;  //!< Maximum departure from stationary protected cores.
  bool protectionChecked = false;     //!< Protected cores were measured, or the request explicitly had none.
  double maxConvergenceErrorMm = 0.0; //!< Difference from the requested finer calculation.
  bool convergenceChecked = false;    //!< An actual refinement comparison was performed.
  bool cellsVerified = false; //!< Producer completed the chosen cell-verification procedure, not just center sampling.
};

/**
 * @brief Provisional engineering limits, not clinical safety limits or a proof of injectivity.
 * @details Both map directions are tested independently. Default cell verification
 * cannot be satisfied by Stage 1's sampled reference alone. A test-only sampled
 * profile may explicitly disable that requirement; its acceptance means only
 * that the requested evidence passed. No option allows detected folds.
 */
struct QualityPolicy
{
  NumericalPolicyVersion version{1};    //!< Nonzero policy identity to persist with future results.
  double minDeterminant = 0.1;          //!< Strictly positive hard compression floor.
  double maxDeterminant = 10.0;         //!< Hard expansion ceiling.
  double minSingularValue = 0.05;       //!< Strictly positive minimum directional stretch.
  double maxSingularValue = 20.0;       //!< Maximum directional stretch.
  double maxResidualMm = 0.1;           //!< Physical inverse-consistency tolerance.
  double maxResidualVoxels = 0.05;      //!< Voxel-normalized inverse-consistency tolerance; both limits apply.
  double maxProtectionErrorMm = 1.0e-8; //!< Protected-core drift tolerance.
  double maxConvergenceErrorMm = 0.01;  //!< Pointwise refinement-difference tolerance.
  bool requireVerifiedCells = true;     //!< Require more than sampled Jacobians before acceptance.
};

/** @brief Numerical decision; callers retain the previous accepted state on either non-accepting outcome. */
enum class CandidateDecision
{
  Accept, //!< The supplied evidence passed the requested profile.
  Refine, //!< Obtain better resolution/evidence before considering publication.
  Reject  //!< The proposed motion or diagnostic data are unacceptable.
};

/** @brief Most urgent reason for a candidate decision. */
enum class QualityReason
{
  Passed,             //!< Requested evidence and thresholds passed, not a global diffeomorphism certificate.
  MissingEvidence,    //!< Empty/inconsistent counts or missing convergence/cell checks.
  NonFinite,          //!< Invalid/non-finite diagnostic values.
  Coverage,           //!< Some requested samples could not be evaluated.
  Folding,            //!< A sampled determinant is nonpositive.
  Distortion,         //!< Compression, expansion, or singular values violate the policy.
  InverseConsistency, //!< Either inverse residual exceeds its bound.
  Protection,         //!< A protected core moved too far.
  Convergence         //!< Refinement difference exceeds its bound.
};

/** @brief Decision plus a machine-readable explanation suitable for later retry/UI policy. */
struct CandidateAssessment
{
  CandidateDecision decision; //!< Accept, request better numerical evidence, or reject motion.
  QualityReason reason;       //!< Does not require parsing diagnostic text.
};

/** @brief Assess both directions and constraints; invalid policy parameters throw ContractError. */
[[nodiscard]] CandidateAssessment assessCandidate(const QualityReport& report, const QualityPolicy& policy = {});
} // namespace deformation
