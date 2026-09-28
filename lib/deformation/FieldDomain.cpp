#include "deformation/FieldDomain.h"

#include "deformation/ContractError.h"

#include <glm/geometric.hpp>
#include <glm/matrix.hpp>

#include <cmath>
#include <limits>

namespace deformation
{
namespace
{
bool finite(const glm::dvec3& value)
{
  return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

void requireFinite(const glm::dvec3& value)
{
  if (!finite(value)) {
    throw ContractError(ContractFailure::NonFiniteCoordinate, "Coordinates and converted results must be finite");
  }
}
} // namespace

FieldDomain::FieldDomain(const DomainGeometry& geometry)
  : m_geometry(geometry), m_validExtent(geometry.validExtent.value_or(IndexExtent{{0, 0, 0}, geometry.size}))
{
  if (geometry.dimension != SpatialDimension::Plane && geometry.dimension != SpatialDimension::Volume) {
    throw ContractError(ContractFailure::InvalidDimension, "Only native 2D and spatial 3D domains are supported");
  }
  if (!finite(geometry.origin)) {
    throw ContractError(ContractFailure::InvalidOrigin, "The physical origin must be finite");
  }
  if (!finite(geometry.spacing) || geometry.spacing.x <= 0.0 || geometry.spacing.y <= 0.0 || geometry.spacing.z <= 0.0)
  {
    throw ContractError(ContractFailure::InvalidSpacing, "Sample spacing must be finite and positive");
  }
  for (const auto count : geometry.size) {
    if (count == 0) {
      throw ContractError(ContractFailure::EmptyDomain, "Each stored dimension must contain at least one sample");
    }
    if (m_sampleCount > std::numeric_limits<std::size_t>::max() / count) {
      throw ContractError(ContractFailure::SampleCountOverflow, "The stored sample count exceeds size_t");
    }
    m_sampleCount *= count;
  }
  if (geometry.dimension == SpatialDimension::Plane && (geometry.size[2] != 1 || geometry.spacing.z != 1.0)) {
    throw ContractError(ContractFailure::InvalidPlanarGeometry, "Native 2D requires z size 1 and inactive z spacing 1");
  }
  for (glm::length_t axis = 0; axis < 3; ++axis) {
    if (!finite(geometry.directions[axis])) {
      throw ContractError(ContractFailure::InvalidDirections, "Direction columns must be finite");
    }
    for (glm::length_t other = 0; other < 3; ++other) {
      const double expected = axis == other ? 1.0 : 0.0;
      if (std::abs(glm::dot(geometry.directions[axis], geometry.directions[other]) - expected) > directionTolerance) {
        throw ContractError(ContractFailure::InvalidDirections, "Direction columns must be orthonormal");
      }
    }
    const auto i = static_cast<std::size_t>(axis);
    if (m_validExtent.begin[i] >= m_validExtent.end[i] || m_validExtent.end[i] > geometry.size[i]) {
      throw ContractError(ContractFailure::InvalidExtent, "Valid sample extent must be nonempty and inside the grid");
    }
    m_indexToPhysical[axis] = geometry.directions[axis] * geometry.spacing[axis];
    if (!finite(m_indexToPhysical[axis])) {
      throw ContractError(ContractFailure::InvalidSpacing, "Spacing does not admit a finite forward transform");
    }
  }

  // Invert directions before dividing by spacing to avoid underflow/overflow
  // in the determinant of a highly anisotropic physical-index matrix.
  m_physicalToIndex = glm::inverse(geometry.directions);
  for (glm::length_t column = 0; column < 3; ++column) {
    m_physicalToIndex[column] /= geometry.spacing;
    if (!finite(m_physicalToIndex[column])) {
      throw ContractError(ContractFailure::InvalidSpacing, "Spacing does not admit a finite inverse transform");
    }
  }
}

SpatialDimension FieldDomain::dimension() const noexcept
{
  return m_geometry.dimension;
}

const GridSize& FieldDomain::size() const noexcept
{
  return m_geometry.size;
}

const IndexExtent& FieldDomain::validExtent() const noexcept
{
  return m_validExtent;
}

const glm::dvec3& FieldDomain::origin() const noexcept
{
  return m_geometry.origin;
}

const glm::dvec3& FieldDomain::spacing() const noexcept
{
  return m_geometry.spacing;
}

const glm::dmat3& FieldDomain::directions() const noexcept
{
  return m_geometry.directions;
}

std::size_t FieldDomain::sampleCount() const noexcept
{
  return m_sampleCount;
}

glm::dvec3 FieldDomain::indexToPhysical(const glm::dvec3& index) const
{
  const glm::dvec3 result = m_geometry.origin + indexVectorToPhysical(index);
  requireFinite(result);
  return result;
}

glm::dvec3 FieldDomain::physicalToIndex(const glm::dvec3& point) const
{
  requireFinite(point);
  return physicalVectorToIndex(point - m_geometry.origin);
}

glm::dvec3 FieldDomain::indexVectorToPhysical(const glm::dvec3& offset) const
{
  requireFinite(offset);
  if (dimension() == SpatialDimension::Plane && offset.z != 0.0) {
    throw ContractError(ContractFailure::OutsidePlane, "Native-2D indices and offsets require zero z");
  }
  const glm::dvec3 result = m_indexToPhysical * offset;
  requireFinite(result);
  return result;
}

glm::dvec3 FieldDomain::physicalVectorToIndex(const glm::dvec3& displacement) const
{
  requireFinite(displacement);
  glm::dvec3 result = m_physicalToIndex * displacement;
  requireFinite(result);
  if (dimension() == SpatialDimension::Plane) {
    if (std::abs(result.z) > planeToleranceMm) {
      throw ContractError(ContractFailure::OutsidePlane, "Physical coordinate is outside the native-2D plane");
    }
    result.z = 0.0;
  }
  return result;
}

bool FieldDomain::containsIndex(const glm::dvec3& index) const noexcept
{
  if (!finite(index) || (dimension() == SpatialDimension::Plane && index.z != 0.0)) {
    return false;
  }
  for (glm::length_t axis = 0; axis < 3; ++axis) {
    const auto i = static_cast<std::size_t>(axis);
    if (index[axis] < m_validExtent.begin[i] || index[axis] > m_validExtent.end[i] - 1u) {
      return false;
    }
  }
  return true;
}

} // namespace deformation
