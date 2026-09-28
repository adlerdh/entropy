#include "deformation/VelocityLattice.h"

#include "deformation/ContractError.h"

#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <utility>

namespace deformation
{
namespace
{
FieldDomain latticeDomain(const BrushStep& brush, double spacingMm, std::size_t budget)
{
  if (!std::isfinite(spacingMm) || spacingMm <= 0.0 || budget == 0) {
    throw ContractError(ContractFailure::InvalidLattice, "Lattice spacing and allocation budget must be positive");
  }
  const auto& d = brush.definition();
  const double halfSize = std::ceil(d.radiusMm / spacingMm) + 2.0;
  const double count = 2.0 * halfSize + 1.0;
  if (!std::isfinite(count) || count > std::numeric_limits<std::uint32_t>::max()) {
    throw ContractError(ContractFailure::InvalidLattice, "Requested lattice dimension is too large");
  }
  DomainGeometry geometry;
  geometry.dimension = d.dimension;
  geometry.directions = d.directions;
  geometry.size = {
    static_cast<std::uint32_t>(count),
    static_cast<std::uint32_t>(count),
    d.dimension == SpatialDimension::Plane ? 1u : static_cast<std::uint32_t>(count)};
  geometry.spacing = {spacingMm, spacingMm, d.dimension == SpatialDimension::Plane ? 1.0 : spacingMm};
  const glm::dvec3 offset{
    halfSize * spacingMm,
    halfSize * spacingMm,
    d.dimension == SpatialDimension::Plane ? 0.0 : halfSize * spacingMm};
  geometry.origin = d.centerMm - d.directions * offset;
  FieldDomain domain{geometry};
  if (
    domain.sampleCount() > budget ||
    domain.sampleCount() > std::numeric_limits<std::size_t>::max() / sizeof(glm::dvec3))
  {
    throw ContractError(ContractFailure::InvalidLattice, "Requested lattice exceeds its coefficient allocation budget");
  }
  return domain;
}

std::array<double, 4> weights(double t)
{
  const double s = 1.0 - t;
  return {
    s * s * s / 6.0,
    (3.0 * t * t * t - 6.0 * t * t + 4.0) / 6.0,
    (-3.0 * t * t * t + 3.0 * t * t + 3.0 * t + 1.0) / 6.0,
    t * t * t / 6.0};
}
} // namespace

VelocityLattice::VelocityLattice(BrushStep brush, double spacingMm, std::size_t maxCoefficients)
  : m_brush(std::move(brush)), m_domain(latticeDomain(m_brush, spacingMm, maxCoefficients))
{
  m_coefficients.reserve(m_domain.sampleCount());
  for (std::uint32_t z = 0; z < m_domain.size()[2]; ++z) {
    for (std::uint32_t y = 0; y < m_domain.size()[1]; ++y) {
      for (std::uint32_t x = 0; x < m_domain.size()[0]; ++x) {
        m_coefficients.push_back(m_brush.generator(m_domain.indexToPhysical(glm::dvec3{x, y, z})));
      }
    }
  }
}

const FieldDomain& VelocityLattice::domain() const noexcept
{
  return m_domain;
}

std::span<const glm::dvec3> VelocityLattice::coefficients() const noexcept
{
  return m_coefficients;
}

glm::dvec3 VelocityLattice::velocity(const glm::dvec3& pointMm) const
{
  const double envelope = m_brush.envelope(pointMm);
  if (envelope == 0.0) return glm::dvec3{0.0};
  const glm::dvec3 index = m_domain.physicalToIndex(pointMm);
  std::array<std::size_t, 3> base{};
  std::array<std::array<double, 4>, 3> w{};
  const int dimensions = m_domain.dimension() == SpatialDimension::Plane ? 2 : 3;
  for (int axis = 0; axis < dimensions; ++axis) {
    const double cell = std::floor(index[axis]);
    const auto i = static_cast<std::size_t>(axis);
    if (cell < 1.0 || cell + 2.0 >= m_domain.size()[i]) {
      throw ContractError(ContractFailure::InvalidLattice, "Brush query lacks a complete coefficient halo");
    }
    base[i] = static_cast<std::size_t>(cell) - 1;
    w[i] = weights(index[axis] - cell);
  }
  if (dimensions == 2) w[2] = {1.0, 0.0, 0.0, 0.0};
  glm::dvec3 result{0.0};
  const std::size_t zCount = dimensions == 2 ? 1 : 4;
  for (std::size_t z = 0; z < zCount; ++z) {
    for (std::size_t y = 0; y < 4; ++y) {
      for (std::size_t x = 0; x < 4; ++x) {
        const std::size_t address =
          base[0] + x +
          m_domain.size()[0] * (base[1] + y + static_cast<std::size_t>(m_domain.size()[1]) * (base[2] + z));
        result += w[0][x] * w[1][y] * w[2][z] * m_coefficients[address];
      }
    }
  }
  return envelope * result;
}
} // namespace deformation
