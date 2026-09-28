#pragma once

#include <glm/vec3.hpp>

#include <cstddef>
#include <span>
#include <vector>

namespace deformation
{
/** @brief A pointer location already resolved into physical reference space. */
struct StrokeSample
{
  glm::dvec3 positionMm{0.0}; //!< LPS position, on the selected plane for native 2D.
  double timeSeconds = 0.0;   //!< Monotonic timestamp; equal timestamps require equal positions.
  bool active = true;         //!< False breaks the path; time and movement across that break are ignored.
};

/** @brief One canonical piece of a piecewise-linear active stroke. */
struct StrokeSegment
{
  glm::dvec3 beginMm{0.0};    //!< Last emitted physical position.
  glm::dvec3 endMm{0.0};      //!< Current emitted physical position; the midpoint is a suitable brush center.
  double activeSeconds = 0.0; //!< Active duration; radial/angular rates multiply this value.
};

/** @brief Physical and time sampling limits, independent of pointer-event frequency. */
struct StrokeSamplingOptions
{
  double maxDistanceMm = 1.0;           //!< Maximum accumulated path length per emitted segment.
  double maxActiveSeconds = 1.0 / 60.0; //!< Maximum active duration per emitted segment.
  double maxEventGapSeconds = 0.5;      //!< Larger event gaps break the path rather than generating a catch-up stroke.
  std::size_t maxSegments = 100'000;    //!< Hard output budget; exceeding it throws instead of truncating the stroke.
};

/**
 * @brief Resample an entire captured path into bounded physical/time increments.
 * @details Carries fractional distance/time across input events; collinear
 * constant-speed paths agree across event rates within roundoff. Corners are
 * approximated at the chosen resolution; reversals flush their turning point.
 * Pauses and excessive event gaps never create a connecting segment. Stationary
 * holds emit time segments but imply zero push displacement. A final partial
 * segment is retained. Cancelled paths return no segments and are not evaluated.
 *
 * Push uses endMm - beginMm; radial and angular tools use rate * activeSeconds.
 * This function neither applies strength nor integrates a deformation. Native-2D
 * planarity is validated when constructing the resulting BrushStep recipes.
 * @throws ContractError For invalid samples/options or an exceeded segment budget.
 */
[[nodiscard]] std::vector<StrokeSegment> resampleStroke(
  std::span<const StrokeSample> samples,
  const StrokeSamplingOptions& options = {},
  bool cancelled = false);
} // namespace deformation
