#include "deformation/CellVerification.h"

#include <glm/geometric.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace deformation
{
namespace
{
struct Interval
{
  double low;
  double high;
};
using Corners = std::array<glm::dvec3, 8>;
using JacobianBounds = std::array<std::array<Interval, 3>, 3>; // column, row

double down(double x)
{
  return std::nextafter(x, -std::numeric_limits<double>::infinity());
}
double up(double x)
{
  return std::nextafter(x, std::numeric_limits<double>::infinity());
}
Interval add(Interval a, Interval b)
{
  return {down(a.low + b.low), up(a.high + b.high)};
}
Interval subtract(Interval a, Interval b)
{
  return {down(a.low - b.high), up(a.high - b.low)};
}
Interval multiply(Interval a, Interval b)
{
  const std::array<double, 4> products{a.low * b.low, a.low * b.high, a.high * b.low, a.high * b.high};
  return {down(*std::ranges::min_element(products)), up(*std::ranges::max_element(products))};
}
Interval triple(Interval a, Interval b, Interval c)
{
  return multiply(multiply(a, b), c);
}
Interval determinant(const JacobianBounds& j, int n)
{
  const auto a = j[0][0], b = j[1][0], c = j[2][0];
  const auto d = j[0][1], e = j[1][1], f = j[2][1];
  if (n == 2) return subtract(multiply(a, e), multiply(b, d));
  const auto g = j[0][2], h = j[1][2], i = j[2][2];
  return subtract(
    subtract(add(add(triple(a, e, i), triple(b, f, g)), triple(c, d, h)), triple(c, e, g)),
    add(triple(b, d, i), triple(a, f, h)));
}

glm::dvec3 interpolate(const Corners& corners, const glm::dvec3& t, int n)
{
  glm::dvec3 value(0.0);
  for (int corner = 0; corner < (1 << n); ++corner) {
    double weight = 1.0;
    for (int axis = 0; axis < n; ++axis) {
      weight *= (corner & (1 << axis)) ? t[axis] : 1.0 - t[axis];
    }
    value += weight * corners[corner];
  }
  return value;
}

JacobianBounds jacobianBounds(const Corners& corners, const glm::dmat3& directions, glm::dvec3 span, int n)
{
  JacobianBounds bounds{};
  for (int axis = 0; axis < n; ++axis) {
    for (int row = 0; row < n; ++row) {
      double minimum = std::numeric_limits<double>::infinity();
      double maximum = -std::numeric_limits<double>::infinity();
      for (int corner = 0; corner < (1 << n); ++corner) {
        if (corner & (1 << axis)) continue;
        const auto edge = (corners[corner | (1 << axis)] - corners[corner]) / span[axis];
        const double derivative = glm::dot(directions[row], edge) + (axis == row ? 1.0 : 0.0);
        minimum = std::min(minimum, derivative);
        maximum = std::max(maximum, derivative);
      }
      const double padding =
        64.0 * std::numeric_limits<double>::epsilon() * std::max({1.0, std::abs(minimum), std::abs(maximum)});
      bounds[axis][row] = {down(minimum - padding), up(maximum + padding)};
    }
  }
  return bounds;
}

enum class CellState
{
  Verified,
  Folded,
  Unresolved
};

CellState checkCell(
  const Corners& corners,
  const glm::dmat3& directions,
  glm::dvec3 span,
  const QualityPolicy& policy,
  int n,
  unsigned depth,
  std::size_t& remaining)
{
  if (remaining == 0) return CellState::Unresolved;
  --remaining;
  const auto bounds = jacobianBounds(corners, directions, span, n);
  const auto det = determinant(bounds, n);
  if (!std::isfinite(det.low) || !std::isfinite(det.high)) return CellState::Unresolved;
  if (det.high <= 0.0) return CellState::Folded;
  double frobeniusSquared = 0.0;
  for (int column = 0; column < n; ++column) {
    for (int row = 0; row < n; ++row) {
      const auto entry = bounds[column][row];
      const double absolute = std::max(std::abs(entry.low), std::abs(entry.high));
      frobeniusSquared = up(frobeniusSquared + up(absolute * absolute));
    }
  }
  const double upperStretch = up(std::sqrt(frobeniusSquared)) * 1.000001;
  const double lowerStretch =
    frobeniusSquared > 0.0 ? (n == 2 ? det.low / upperStretch : 2.0 * det.low / frobeniusSquared) * 0.999999 : 0.0;
  if (
    det.low >= policy.minDeterminant && det.high <= policy.maxDeterminant && lowerStretch >= policy.minSingularValue &&
    upperStretch <= policy.maxSingularValue)
  {
    return CellState::Verified;
  }
  if (depth == 0) return CellState::Unresolved;

  CellState combined = CellState::Verified;
  for (int child = 0; child < (1 << n); ++child) {
    Corners subcell{};
    for (int corner = 0; corner < (1 << n); ++corner) {
      glm::dvec3 t(0.0);
      for (int axis = 0; axis < n; ++axis) {
        t[axis] = 0.5 * static_cast<double>(((child >> axis) & 1) + ((corner >> axis) & 1));
      }
      subcell[corner] = interpolate(corners, t, n);
    }
    const auto result = checkCell(subcell, directions, span * 0.5, policy, n, depth - 1, remaining);
    if (result == CellState::Folded) return result;
    if (result == CellState::Unresolved) combined = result;
  }
  return combined;
}

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
} // namespace

CellVerification verifyFieldCells(
  const FieldDomain& domain,
  std::span<const glm::vec4> displacement,
  const QualityPolicy& policy,
  unsigned maxDepth,
  std::size_t maxSamples,
  std::size_t maxSubcells)
{
  static_cast<void>(assessCandidate({}, policy)); // Validate the policy before using its thresholds.
  if (displacement.size() != domain.sampleCount() || domain.sampleCount() > maxSamples || maxDepth > 6) {
    throw std::invalid_argument("Cell verification input or work limit is invalid");
  }
  const int n = domain.dimension() == SpatialDimension::Plane ? 2 : 3;
  const auto& extent = domain.validExtent();
  for (int axis = 0; axis < n; ++axis) {
    if (extent.end[axis] - extent.begin[axis] < 2) return {};
  }
  CellVerification report;
  std::size_t remaining = maxSubcells;
  const std::uint32_t zEnd = n == 2 ? 1 : extent.end[2] - 1;
  for (std::uint32_t z = extent.begin[2]; z < zEnd; ++z) {
    for (std::uint32_t y = extent.begin[1]; y + 1 < extent.end[1]; ++y) {
      for (std::uint32_t x = extent.begin[0]; x + 1 < extent.end[0]; ++x) {
        ++report.requested;
        Corners corners{};
        bool valid = true;
        for (int corner = 0; corner < (1 << n); ++corner) {
          const auto& value = displacement[offset(
            domain,
            x + static_cast<std::uint32_t>(corner & 1),
            y + static_cast<std::uint32_t>((corner >> 1) & 1),
            z + static_cast<std::uint32_t>((corner >> 2) & 1))];
          valid = valid && usable(value, domain);
          if (valid) corners[corner] = glm::dvec3(value);
        }
        if (!valid) {
          ++report.invalid;
          continue;
        }
        switch (checkCell(corners, domain.directions(), domain.spacing(), policy, n, maxDepth, remaining)) {
          case CellState::Verified:
            ++report.verified;
            break;
          case CellState::Folded:
            ++report.folded;
            break;
          case CellState::Unresolved:
            ++report.unresolved;
            break;
        }
      }
    }
  }
  return report;
}
} // namespace deformation
