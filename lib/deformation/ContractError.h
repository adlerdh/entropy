#pragma once

#include <stdexcept>

namespace deformation
{

/** @brief Machine-readable reasons for rejecting geometry or map metadata. */
enum class ContractFailure
{
  InvalidDimension,      //!< Only native 2D and spatial 3D domains are supported.
  EmptyDomain,           //!< Every stored axis must contain at least one sample.
  SampleCountOverflow,   //!< The sample count cannot be represented by std::size_t.
  InvalidSpacing,        //!< Spacing must be finite and strictly positive.
  InvalidOrigin,         //!< Physical origins must be finite.
  InvalidDirections,     //!< Direction columns must form a finite orthonormal frame.
  InvalidPlanarGeometry, //!< Native 2D requires one stored z sample and unit inactive spacing.
  InvalidExtent,         //!< The valid sample extent must be nonempty and inside the stored grid.
  NonFiniteCoordinate,   //!< A coordinate or its converted result is not finite.
  OutsidePlane,          //!< A native-2D coordinate or displacement leaves its physical plane.
  IncompatibleDomains,   //!< Paired maps must have the same spatial dimensionality.
  InvalidRevision,       //!< Revision zero is reserved and cannot identify a map pair.
  InvalidPolicyVersion,  //!< Numerical policy version zero is reserved.
  InvalidMapDirection,   //!< A map direction is not one of the supported enum values.
  InvalidBrush,          //!< Brush support, motion, strength, or protection is invalid.
  InvalidLattice,        //!< A lattice cannot cover the requested brush within its allocation budget.
  InvalidStroke,         //!< Stroke samples or resampling limits violate their contract.
  InvalidQualityPolicy,  //!< Numerical acceptance thresholds are inconsistent or non-finite.
  InvalidIntegration     //!< Reference integration parameters are invalid.
};

/**
 * @brief A violated public API precondition, with a stable typed reason.
 * @details This is not an integration failure or a numerical-quality report.
 * Callers can catch std::invalid_argument or inspect reason() without parsing text.
 */
class ContractError final : public std::invalid_argument
{
public:
  /** @brief Construct an error with a diagnostic message copied by the base class. */
  ContractError(ContractFailure reason, const char* message);

  /** @brief Return the contract that was violated. */
  [[nodiscard]] ContractFailure reason() const noexcept;

private:
  ContractFailure m_reason;
};

} // namespace deformation
