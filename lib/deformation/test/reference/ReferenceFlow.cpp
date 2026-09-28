#include "ReferenceFlow.h"

#include "deformation/ContractError.h"

#include <glm/geometric.hpp>

#include <algorithm>
#include <cmath>
#include <numeric>

namespace deformation::reference
{
namespace
{
glm::dvec3 checked(glm::dvec3 point)
{
  if (!std::isfinite(point.x) || !std::isfinite(point.y) || !std::isfinite(point.z)) {
    throw ContractError(ContractFailure::NonFiniteCoordinate, "Reference integration produced a non-finite coordinate");
  }
  return point;
}

double sign(MapDirection direction)
{
  if (direction == MapDirection::Forward) return 1.0;
  if (direction == MapDirection::Inverse) return -1.0;
  throw ContractError(ContractFailure::InvalidMapDirection, "Unknown reference integration direction");
}
} // namespace

glm::dvec3 integrate(const VectorFunction& velocity, glm::dvec3 pointMm, std::size_t steps, MapDirection direction)
{
  if (!velocity || steps == 0 || steps > 1'048'576) {
    throw ContractError(
      ContractFailure::InvalidIntegration,
      "Reference integration requires a callback and a bounded step count");
  }
  pointMm = checked(pointMm);
  const double h = sign(direction) / static_cast<double>(steps);
  for (std::size_t i = 0; i < steps; ++i) {
    const glm::dvec3 k1 = checked(velocity(pointMm));
    const glm::dvec3 k2 = checked(velocity(checked(pointMm + 0.5 * h * k1)));
    const glm::dvec3 k3 = checked(velocity(checked(pointMm + 0.5 * h * k2)));
    const glm::dvec3 k4 = checked(velocity(checked(pointMm + h * k3)));
    pointMm = checked(pointMm + (h / 6.0) * (k1 + 2.0 * k2 + 2.0 * k3 + k4));
  }
  return pointMm;
}

FlowEstimate integrateConverged(
  const VectorFunction& velocity,
  const glm::dvec3& pointMm,
  const IntegrationOptions& options,
  MapDirection direction)
{
  if (
    options.initialSteps == 0 || options.initialSteps > options.maxSteps / 2 || options.maxSteps > 1'048'576 ||
    !std::isfinite(options.toleranceMm) || options.toleranceMm <= 0.0)
  {
    throw ContractError(ContractFailure::InvalidIntegration, "Invalid reference convergence budget or tolerance");
  }
  FlowEstimate result;
  result.steps = options.initialSteps;
  result.pointMm = integrate(velocity, pointMm, result.steps, direction);
  while (result.steps < options.maxSteps) {
    result.steps = std::min(result.steps * 2, options.maxSteps);
    const glm::dvec3 next = integrate(velocity, pointMm, result.steps, direction);
    const glm::dvec3 difference = next - result.pointMm;
    result.refinementDifferenceMm = std::hypot(difference.x, difference.y, difference.z);
    result.pointMm = next;
    if (result.refinementDifferenceMm <= options.toleranceMm) {
      result.converged = true;
      break;
    }
  }
  return result;
}

glm::dvec3
composeFlows(std::span<const VectorFunction> velocities, glm::dvec3 pointMm, std::size_t steps, MapDirection direction)
{
  (void)sign(direction);
  pointMm = checked(pointMm);
  const auto applyStep = [steps, direction](const glm::dvec3& point, const VectorFunction& velocity) {
    return integrate(velocity, point, steps, direction);
  };
  if (direction == MapDirection::Forward) {
    return std::accumulate(velocities.begin(), velocities.end(), pointMm, applyStep);
  }
  return std::accumulate(velocities.rbegin(), velocities.rend(), pointMm, applyStep);
}

glm::dmat3
physicalJacobian(const VectorFunction& map, const glm::dvec3& pointMm, const FieldDomain& domain, double stepMm)
{
  if (!map || !std::isfinite(stepMm) || stepMm <= 0.0) {
    throw ContractError(
      ContractFailure::InvalidIntegration,
      "Jacobian differences require a map and positive physical step");
  }
  (void)domain.physicalToIndex(pointMm);
  const int n = domain.dimension() == SpatialDimension::Plane ? 2 : 3;
  glm::dmat3 result{0.0};
  for (int c = 0; c < n; ++c) {
    const glm::dvec3 delta = stepMm * domain.directions()[c];
    const glm::dvec3 plus = checked(map(checked(pointMm + delta)));
    const glm::dvec3 minus = checked(map(checked(pointMm - delta)));
    (void)domain.physicalToIndex(plus);
    (void)domain.physicalToIndex(minus);
    const glm::dvec3 derivative = checked((plus - minus) / (2.0 * stepMm));
    for (int r = 0; r < n; ++r)
      result[c][r] = glm::dot(derivative, domain.directions()[r]);
  }
  return result;
}
} // namespace deformation::reference
