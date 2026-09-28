#pragma once

#include "deformation/FieldDomain.h"
#include "deformation/Types.h"

#include <glm/mat3x3.hpp>
#include <glm/vec3.hpp>

#include <cstddef>
#include <functional>
#include <span>

namespace deformation::reference
{
/** @brief Physical velocity or point-map callback, evaluated in double precision. */
using VectorFunction = std::function<glm::dvec3(const glm::dvec3&)>;

/** @brief Bounded convergence experiment; this oracle is test support, not an interactive backend. */
struct IntegrationOptions
{
  std::size_t initialSteps = 4; //!< Initial uniform RK4 subdivision of normalized time [0, 1].
  std::size_t maxSteps = 4096;  //!< Maximum subdivisions; at least twice initialSteps.
  double toleranceMm = 1.0e-9;  //!< Maximum change between consecutive refinements.
};

/** @brief Reference integration result; callers must inspect converged before using it as an oracle. */
struct FlowEstimate
{
  glm::dvec3 pointMm{0.0};             //!< Last computed endpoint.
  double refinementDifferenceMm = 0.0; //!< Observed difference, not a rigorous global error bound.
  std::size_t steps = 0;               //!< Subdivisions used for the endpoint.
  bool converged = false;              //!< Observed refinement difference satisfied the requested tolerance.
};

/** @brief Integrate a stationary velocity over [0, 1] with classical RK4; inverse integrates negative velocity. */
[[nodiscard]] glm::dvec3 integrate(
  const VectorFunction& velocity,
  glm::dvec3 pointMm,
  std::size_t steps,
  MapDirection direction = MapDirection::Forward);

/** @brief Double RK4 resolution until the endpoint stabilizes or the budget is exhausted. */
[[nodiscard]] FlowEstimate integrateConverged(
  const VectorFunction& velocity,
  const glm::dvec3& pointMm,
  const IntegrationOptions& options = {},
  MapDirection direction = MapDirection::Forward);

/** @brief Integrate a chronological sequence of edits; inverse uses reverse order and negative velocities. */
[[nodiscard]] glm::dvec3 composeFlows(
  std::span<const VectorFunction> velocities,
  glm::dvec3 pointMm,
  std::size_t steps,
  MapDirection direction = MapDirection::Forward);

/**
 * @brief Central physical differences of an analytic map in the supplied domain frame.
 * @details Returns intrinsic derivatives. Native 2D uses a 2-by-2 tangent block
 * with zero unused entries, not a fake normal derivative. The callback must be
 * valid on the entire difference stencil and map a native-2D domain back into
 * its own plane; this does not check field coverage.
 */
[[nodiscard]] glm::dmat3
physicalJacobian(const VectorFunction& map, const glm::dvec3& pointMm, const FieldDomain& domain, double stepMm);
} // namespace deformation::reference
