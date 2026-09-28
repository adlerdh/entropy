#include "deformation/BrushStep.h"

#include "deformation/ContractError.h"

#include <glm/geometric.hpp>

#include <cmath>
#include <type_traits>
#include <utility>

namespace deformation
{
namespace
{
double length(const glm::dvec3& value)
{
  return std::hypot(value.x, value.y, value.z);
}

FieldDomain frame(const BrushDefinition& definition)
{
  DomainGeometry geometry;
  geometry.dimension = definition.dimension;
  geometry.origin = definition.centerMm;
  geometry.directions = definition.directions;
  return FieldDomain{geometry};
}

double smoothstep(double t)
{
  return t * t * t * (10.0 + t * (-15.0 + 6.0 * t));
}
} // namespace

BrushStep::BrushStep(BrushDefinition definition) : m_definition(std::move(definition)), m_frame(frame(m_definition))
{
  const auto& d = m_definition;
  if (!std::isfinite(d.radiusMm) || d.radiusMm <= 0.0 || !std::isfinite(d.strength) || d.strength < 0.0) {
    throw ContractError(
      ContractFailure::InvalidBrush,
      "Brush radius must be positive and strength nonnegative and finite");
  }
  std::visit(
    [&](const auto& motion) {
      using Motion = std::decay_t<decltype(motion)>;
      if constexpr (std::is_same_v<Motion, PushMotion>) {
        (void)m_frame.physicalVectorToIndex(motion.displacementMm);
      }
      else if constexpr (std::is_same_v<Motion, RadialMotion>) {
        if (!std::isfinite(motion.exposure)) {
          throw ContractError(ContractFailure::InvalidBrush, "Radial exposure must be finite");
        }
      }
      else {
        if (
          !std::isfinite(motion.angleRadians) || !std::isfinite(length(motion.axis)) ||
          std::abs(length(motion.axis) - 1.0) > FieldDomain::directionTolerance)
        {
          throw ContractError(ContractFailure::InvalidBrush, "Twirl requires a finite angle and a unit axis");
        }
        if (
          d.dimension == SpatialDimension::Plane &&
          length(glm::cross(motion.axis, d.directions[2])) > FieldDomain::directionTolerance)
        {
          throw ContractError(ContractFailure::InvalidBrush, "A native-2D twirl axis must be normal to its plane");
        }
      }
    },
    d.motion);
  for (const auto& region : d.protection) {
    (void)m_frame.physicalToIndex(region.centerMm);
    if (
      !std::isfinite(region.coreRadiusMm) || region.coreRadiusMm < 0.0 || !std::isfinite(region.transitionMm) ||
      region.transitionMm <= 0.0)
    {
      throw ContractError(
        ContractFailure::InvalidBrush,
        "Protection requires a nonnegative core and positive transition");
    }
  }
}

const BrushDefinition& BrushStep::definition() const noexcept
{
  return m_definition;
}

glm::dvec3 BrushStep::generator(const glm::dvec3& pointMm) const
{
  // The frame conversion also checks finite coordinates and native-2D planarity.
  const glm::dvec3 relative = m_frame.indexVectorToPhysical(m_frame.physicalToIndex(pointMm));
  glm::dvec3 result = std::visit(
    [&](const auto& motion) -> glm::dvec3 {
      using Motion = std::decay_t<decltype(motion)>;
      if constexpr (std::is_same_v<Motion, PushMotion>) {
        return motion.displacementMm;
      }
      else if constexpr (std::is_same_v<Motion, RadialMotion>) {
        return motion.exposure * relative;
      }
      else {
        return motion.angleRadians * glm::cross(motion.axis, relative);
      }
    },
    m_definition.motion);
  result *= m_definition.strength;
  return m_frame.indexVectorToPhysical(m_frame.physicalVectorToIndex(result));
}

double BrushStep::envelope(const glm::dvec3& pointMm) const
{
  const glm::dvec3 relative = m_frame.indexVectorToPhysical(m_frame.physicalToIndex(pointMm));
  const double r = length(relative) / m_definition.radiusMm;
  if (r >= 1.0) return 0.0;
  const double s = 1.0 - r;
  double weight = s * s * s * s * (4.0 * r + 1.0);
  for (const auto& region : m_definition.protection) {
    const double distance = length(pointMm - region.centerMm);
    if (distance <= region.coreRadiusMm) return 0.0;
    const double t = (distance - region.coreRadiusMm) / region.transitionMm;
    if (t < 1.0) weight *= smoothstep(t);
  }
  return weight;
}

glm::dvec3 BrushStep::velocity(const glm::dvec3& pointMm) const
{
  const double weight = envelope(pointMm);
  return weight == 0.0 ? glm::dvec3{0.0} : weight * generator(pointMm);
}
} // namespace deformation
