#include "deformation/FieldEvidence.h"

#include "deformation/ContractError.h"

#include <glm/geometric.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>

namespace deformation
{
namespace
{
std::size_t offset(const FieldDomain& domain, std::uint32_t x, std::uint32_t y, std::uint32_t z)
{
  return (static_cast<std::size_t>(z) * domain.size()[1] + y) * domain.size()[0] + x;
}
bool usable(const glm::vec4& value, const FieldDomain& domain)
{
  if (
    value.w != 1.0f || !std::isfinite(value.x) || !std::isfinite(value.y) || !std::isfinite(value.z) ||
    !std::isfinite(value.w))
  {
    return false;
  }
  return domain.dimension() == SpatialDimension::Volume ||
         std::abs(glm::dot(domain.directions()[2], glm::dvec3(value))) <= FieldDomain::planeToleranceMm;
}
using Corners = std::array<glm::dvec3, 8>;
glm::dvec3 interpolate(const Corners& corners, glm::dvec3 t, int n)
{
  glm::dvec3 result(0.0);
  for (int corner = 0; corner < (1 << n); ++corner) {
    double weight = 1.0;
    for (int axis = 0; axis < n; ++axis)
      weight *= (corner & (1 << axis)) ? t[axis] : 1.0 - t[axis];
    result += weight * corners[corner];
  }
  return result;
}
bool intersects(glm::dvec3 centerIndex, double radiusMm, glm::dvec3 spacing, glm::dvec3 low, glm::dvec3 high, int n)
{
  double distanceSquared = 0.0;
  for (int axis = 0; axis < n; ++axis) {
    const double nearest = std::clamp(centerIndex[axis], low[axis], high[axis]);
    const double delta = (nearest - centerIndex[axis]) * spacing[axis];
    distanceSquared += delta * delta;
  }
  return distanceSquared <= radiusMm * radiusMm;
}
struct BoundResult
{
  double maximum = 0.0;
  bool complete = true;
};
BoundResult boundCoreCell(
  const Corners& corners,
  glm::dvec3 base,
  glm::dvec3 low,
  glm::dvec3 high,
  glm::dvec3 centerIndex,
  double radiusMm,
  glm::dvec3 spacing,
  double targetMm,
  int n,
  unsigned depth,
  std::size_t& remaining)
{
  if (!intersects(centerIndex, radiusMm, spacing, base + low, base + high, n)) return {};
  if (remaining == 0) return {0.0, false};
  --remaining;
  double bound = 0.0;
  for (int corner = 0; corner < (1 << n); ++corner) {
    glm::dvec3 t = low;
    for (int axis = 0; axis < n; ++axis) {
      if (corner & (1 << axis)) t[axis] = high[axis];
    }
    bound = std::max(bound, glm::length(interpolate(corners, t, n)));
  }
  // Convex interpolation cannot exceed the largest corner-vector norm.
  if (bound <= targetMm || depth == 0) return {bound, true};
  const glm::dvec3 middle = 0.5 * (low + high);
  BoundResult result;
  for (int child = 0; child < (1 << n); ++child) {
    glm::dvec3 childLow = low;
    glm::dvec3 childHigh = high;
    for (int axis = 0; axis < n; ++axis) {
      if (child & (1 << axis))
        childLow[axis] = middle[axis];
      else
        childHigh[axis] = middle[axis];
    }
    const auto part = boundCoreCell(
      corners,
      base,
      childLow,
      childHigh,
      centerIndex,
      radiusMm,
      spacing,
      targetMm,
      n,
      depth - 1,
      remaining);
    result.maximum = std::max(result.maximum, part.maximum);
    result.complete = result.complete && part.complete;
    if (!result.complete) return result;
  }
  return result;
}
} // namespace

std::optional<glm::dvec3> sampleDisplacement(SampledFieldView field, const glm::dvec3& pointMm)
{
  const auto& domain = field.domain;
  if (field.values.size() != domain.sampleCount()) throw std::invalid_argument("Field sample count mismatch");
  glm::dvec3 index;
  try {
    index = domain.physicalToIndex(pointMm);
  }
  catch (const ContractError&) {
    return std::nullopt;
  }
  const int n = domain.dimension() == SpatialDimension::Plane ? 2 : 3;
  const auto& extent = domain.validExtent();
  std::array<std::uint32_t, 3> lo{0, 0, 0};
  std::array<std::uint32_t, 3> hi{0, 0, 0};
  glm::dvec3 fraction(0.0);
  for (int axis = 0; axis < n; ++axis) {
    const double lower = static_cast<double>(extent.begin[axis]);
    const double upper = static_cast<double>(extent.end[axis] - 1);
    if (index[axis] < lower - 1e-8 || index[axis] > upper + 1e-8) return std::nullopt;
    double q = std::clamp(index[axis], lower, upper);
    if (std::abs(q - std::round(q)) <= 1e-8) q = std::round(q);
    lo[axis] = static_cast<std::uint32_t>(std::floor(q));
    hi[axis] = std::min(lo[axis] + 1, extent.end[axis] - 1);
    fraction[axis] = q - static_cast<double>(lo[axis]);
  }
  glm::dvec3 result(0.0);
  for (int corner = 0; corner < (1 << n); ++corner) {
    double weight = 1.0;
    std::array<std::uint32_t, 3> at = lo;
    for (int axis = 0; axis < n; ++axis) {
      const bool high = (corner & (1 << axis)) != 0;
      weight *= high ? fraction[axis] : 1.0 - fraction[axis];
      if (high) at[axis] = hi[axis];
    }
    if (weight == 0.0) continue;
    const auto& value = field.values[offset(domain, at[0], at[1], at[2])];
    if (!usable(value, domain)) return std::nullopt;
    result += weight * glm::dvec3(value);
  }
  return result;
}

RefinementEvidence compareRefinement(SampledFieldPair candidate, SampledFieldPair refined, std::size_t maxPoints)
{
  RefinementEvidence result;
  const auto compare = [&](SampledFieldView coarse, SampledFieldView fine) {
    if (coarse.domain.dimension() != fine.domain.dimension()) {
      throw std::invalid_argument("Refinement dimensions must match");
    }
    const int n = coarse.domain.dimension() == SpatialDimension::Plane ? 2 : 3;
    const auto& extent = coarse.domain.validExtent();
    const auto observe = [&](glm::dvec3 index) {
      if (result.requested == maxPoints) throw std::invalid_argument("Refinement sample budget exceeded");
      ++result.requested;
      const auto point = coarse.domain.indexToPhysical(index);
      const auto first = sampleDisplacement(coarse, point);
      const auto second = sampleDisplacement(fine, point);
      if (!first || !second) {
        ++result.unavailable;
        return;
      }
      result.maxErrorMm = std::max(result.maxErrorMm, glm::length(*first - *second));
    };
    for (std::uint32_t z = extent.begin[2]; z < extent.end[2]; ++z) {
      for (std::uint32_t y = extent.begin[1]; y < extent.end[1]; ++y) {
        for (std::uint32_t x = extent.begin[0]; x < extent.end[0]; ++x) {
          observe({x, y, z});
          if (x + 1 < extent.end[0] && y + 1 < extent.end[1] && (n == 2 || z + 1 < extent.end[2])) {
            observe({x + 0.5, y + 0.5, n == 2 ? 0.0 : z + 0.5});
          }
        }
      }
    }
  };
  compare(candidate.forward, refined.forward);
  compare(candidate.inverse, refined.inverse);
  return result;
}

ProtectionEvidence measureProtectedCores(
  SampledFieldPair increment,
  std::span<const ProtectedRegion> regions,
  double targetErrorMm,
  unsigned maxDepth,
  std::size_t maxCells)
{
  if (!std::isfinite(targetErrorMm) || targetErrorMm < 0.0 || maxDepth > 8) {
    throw std::invalid_argument("Protection verification limits are invalid");
  }
  ProtectionEvidence result;
  if (regions.empty()) {
    result.checked = true;
    return result;
  }
  std::size_t remaining = maxCells;
  const auto measure = [&](SampledFieldView field) {
    const auto& domain = field.domain;
    if (field.values.size() != domain.sampleCount()) throw std::invalid_argument("Field sample count mismatch");
    const int n = domain.dimension() == SpatialDimension::Plane ? 2 : 3;
    const auto& extent = domain.validExtent();
    for (const auto& region : regions) {
      const std::size_t before = result.intersectingCells;
      if (!std::isfinite(region.coreRadiusMm) || region.coreRadiusMm < 0.0) return false;
      glm::dvec3 center;
      try {
        center = domain.physicalToIndex(region.centerMm);
      }
      catch (const ContractError&) {
        return false;
      }
      for (int axis = 0; axis < n; ++axis) {
        const double radiusIndex = region.coreRadiusMm / domain.spacing()[axis];
        if (center[axis] - radiusIndex < extent.begin[axis] || center[axis] + radiusIndex > extent.end[axis] - 1) {
          return false;
        }
      }
      const std::uint32_t zEnd = n == 2 ? 1 : extent.end[2] - 1;
      for (std::uint32_t z = extent.begin[2]; z < zEnd; ++z) {
        for (std::uint32_t y = extent.begin[1]; y + 1 < extent.end[1]; ++y) {
          for (std::uint32_t x = extent.begin[0]; x + 1 < extent.end[0]; ++x) {
            const glm::dvec3 base(x, y, z);
            const glm::dvec3 high = base + glm::dvec3(1.0, 1.0, n == 2 ? 0.0 : 1.0);
            if (!intersects(center, region.coreRadiusMm, domain.spacing(), base, high, n)) continue;
            ++result.intersectingCells;
            Corners corners{};
            for (int corner = 0; corner < (1 << n); ++corner) {
              const auto& value = field.values[offset(
                domain,
                x + static_cast<std::uint32_t>(corner & 1),
                y + static_cast<std::uint32_t>((corner >> 1) & 1),
                z + static_cast<std::uint32_t>((corner >> 2) & 1))];
              if (!usable(value, domain)) return false;
              corners[corner] = glm::dvec3(value);
            }
            const auto bound = boundCoreCell(
              corners,
              base,
              glm::dvec3(0.0),
              glm::dvec3(1.0, 1.0, n == 2 ? 0.0 : 1.0),
              center,
              region.coreRadiusMm,
              domain.spacing(),
              targetErrorMm,
              n,
              maxDepth,
              remaining);
            if (!bound.complete) return false;
            result.maxErrorMm = std::max(result.maxErrorMm, bound.maximum);
          }
        }
      }
      if (result.intersectingCells == before) return false;
    }
    return true;
  };
  result.checked = measure(increment.forward) && measure(increment.inverse);
  return result;
}
} // namespace deformation
