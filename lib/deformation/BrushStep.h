#pragma once

#include "deformation/FieldDomain.h"

#include <glm/mat3x3.hpp>
#include <glm/vec3.hpp>

#include <variant>
#include <vector>

namespace deformation
{
/** @brief A drag increment in physical LPS millimeters; a stationary pointer produces zero motion. */
struct PushMotion
{
  glm::dvec3 displacementMm{0.0}; //!< Requested translation before strength and spatial weighting.
};

/** @brief Dimensionless expansion exposure: rate per second times active duration in seconds. */
struct RadialMotion
{
  double exposure = 0.0; //!< Positive inflates, negative deflates; this is a velocity coefficient, not a scale factor.
};

/** @brief Angular exposure about a physical axis: radians per second times active duration. */
struct TwirlMotion
{
  glm::dvec3 axis{0.0, 0.0, 1.0}; //!< Unit LPS axis; native 2D requires either orientation of its plane normal.
  double angleRadians = 0.0;      //!< Signed angular exposure before strength and spatial weighting.
};

/** @brief Supported affine velocity generators; unused tool parameters cannot coexist. */
using BrushMotion = std::variant<PushMotion, RadialMotion, TwirlMotion>;

/**
 * @brief A stationary protected disk (2D) or sphere (3D) in the brush's reference space.
 * @details Velocity is exactly zero inside coreRadiusMm and transitions smoothly
 * to unprotected motion across transitionMm. These are analytic regions, not
 * segmentation masks; arbitrary image-mask interpolation is a later adapter.
 */
struct ProtectedRegion
{
  glm::dvec3 centerMm{0.0};  //!< Center in LPS; a native-2D center must lie in the brush plane.
  double coreRadiusMm = 1.0; //!< Nonnegative radius of the exactly stationary core.
  double transitionMm = 1.0; //!< Strictly positive transition width outside the core.
};

/** @brief Parameters for one stationary velocity integrated over normalized time [0, 1]. */
struct BrushDefinition
{
  SpatialDimension dimension = SpatialDimension::Volume; //!< Native dimension; independent of image sampling.
  glm::dvec3 centerMm{0.0};                              //!< Physical center of the increment.
  glm::dmat3 directions{1.0};        //!< Orthonormal frame; first two columns span a native-2D brush.
  double radiusMm = 10.0;            //!< Strictly positive physical support radius; zero velocity at and beyond it.
  double strength = 1.0;             //!< Nonnegative dimensionless gain, applied to velocity before integration.
  BrushMotion motion = PushMotion{}; //!< Already includes drag distance or active-time exposure.
  std::vector<ProtectedRegion> protection; //!< Immutable reference-space protection for this increment.
};

/**
 * @brief Validated analytic velocity for a canonical brush increment.
 * @details Values are LPS mm per normalized integration time. This object has
 * no relationship to image voxel spacing or display warp strength. Native-2D
 * queries must remain in its plane. Protected regions stay in reference space
 * during this increment; following anatomy requires a newly captured definition.
 */
class BrushStep
{
public:
  /** @brief Copy and validate parameters; invalid input throws ContractError. */
  explicit BrushStep(BrushDefinition definition);
  /** @brief Return the immutable canonical recipe. */
  [[nodiscard]] const BrushDefinition& definition() const noexcept;
  /** @brief Return the unweighted affine velocity, including strength; native-2D results are tangent. */
  [[nodiscard]] glm::dvec3 generator(const glm::dvec3& pointMm) const;
  /**
   * @brief Return compact brush support times all smooth protection envelopes, in [0, 1].
   * @details The radial support is (1-r)^4(4r+1) for r < 1. Protection uses
   * quintic smoothstep across each transition. Invalid/off-plane queries throw ContractError.
   */
  [[nodiscard]] double envelope(const glm::dvec3& pointMm) const;
  /** @brief Evaluate envelope times generator; exactly zero outside support and inside protected cores. */
  [[nodiscard]] glm::dvec3 velocity(const glm::dvec3& pointMm) const;

private:
  BrushDefinition m_definition;
  FieldDomain m_frame;
};
} // namespace deformation
