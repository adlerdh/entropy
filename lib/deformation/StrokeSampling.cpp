#include "deformation/StrokeSampling.h"

#include "deformation/ContractError.h"

#include <glm/geometric.hpp>

#include <algorithm>
#include <cmath>

namespace deformation
{
namespace
{
double distance(const glm::dvec3& a, const glm::dvec3& b)
{
  const auto d = b - a;
  return std::hypot(d.x, d.y, d.z);
}
} // namespace

std::vector<StrokeSegment>
resampleStroke(std::span<const StrokeSample> samples, const StrokeSamplingOptions& options, bool cancelled)
{
  if (cancelled) return {};
  if (
    !std::isfinite(options.maxDistanceMm) || options.maxDistanceMm <= 0.0 || !std::isfinite(options.maxActiveSeconds) ||
    options.maxActiveSeconds <= 0.0 || !std::isfinite(options.maxEventGapSeconds) ||
    options.maxEventGapSeconds <= 0.0 || options.maxSegments == 0)
  {
    throw ContractError(ContractFailure::InvalidStroke, "Stroke limits must be positive and finite");
  }
  for (std::size_t i = 0; i < samples.size(); ++i) {
    const auto& sample = samples[i];
    if (
      !std::isfinite(sample.timeSeconds) || sample.timeSeconds < 0.0 || !std::isfinite(sample.positionMm.x) ||
      !std::isfinite(sample.positionMm.y) || !std::isfinite(sample.positionMm.z))
    {
      throw ContractError(
        ContractFailure::InvalidStroke,
        "Stroke samples require finite positions and nonnegative times");
    }
    if (
      i != 0 && (sample.timeSeconds < samples[i - 1].timeSeconds ||
                 (sample.timeSeconds == samples[i - 1].timeSeconds && sample.positionMm != samples[i - 1].positionMm)))
    {
      throw ContractError(ContractFailure::InvalidStroke, "Stroke time must advance when position changes");
    }
  }
  std::vector<StrokeSegment> result;
  if (samples.empty()) return result;
  glm::dvec3 anchor = samples.front().positionMm;
  glm::dvec3 previousDirection{0.0};
  double accumulatedDistance = 0.0;
  double accumulatedTime = 0.0;
  const auto flush = [&](const glm::dvec3& end) {
    if (accumulatedTime > 0.0 || accumulatedDistance > 0.0) {
      if (result.size() >= options.maxSegments) {
        throw ContractError(ContractFailure::InvalidStroke, "Stroke exceeds its segment budget");
      }
      result.push_back({anchor, end, accumulatedTime});
    }
    anchor = end;
    accumulatedDistance = 0.0;
    accumulatedTime = 0.0;
  };
  for (std::size_t i = 1; i < samples.size(); ++i) {
    const auto& previous = samples[i - 1];
    const auto& sample = samples[i];
    const double duration = sample.timeSeconds - previous.timeSeconds;
    if (!sample.active || !previous.active || duration > options.maxEventGapSeconds) {
      flush(previous.positionMm);
      anchor = sample.positionMm;
      previousDirection = glm::dvec3{0.0};
      continue;
    }
    if (duration == 0.0) continue;
    const double segmentDistance = distance(previous.positionMm, sample.positionMm);
    if (!std::isfinite(segmentDistance)) {
      throw ContractError(ContractFailure::InvalidStroke, "Stroke distance exceeds the physical coordinate range");
    }
    const glm::dvec3 direction =
      segmentDistance == 0.0 ? glm::dvec3{0.0} : (sample.positionMm - previous.positionMm) / segmentDistance;
    if (glm::dot(direction, previousDirection) < 0.0) flush(previous.positionMm);
    if (segmentDistance != 0.0) previousDirection = direction;
    double consumed = 0.0;
    while (consumed < 1.0) {
      double fraction = std::min(1.0 - consumed, (options.maxActiveSeconds - accumulatedTime) / duration);
      if (segmentDistance != 0.0) {
        fraction = std::min(fraction, (options.maxDistanceMm - accumulatedDistance) / segmentDistance);
      }
      if (fraction <= 0.0 || consumed + fraction == consumed) {
        throw ContractError(ContractFailure::InvalidStroke, "Stroke resolution is below floating-point progress");
      }
      consumed = std::min(1.0, consumed + fraction);
      accumulatedDistance += fraction * segmentDistance;
      accumulatedTime += fraction * duration;
      const glm::dvec3 point = previous.positionMm + consumed * (sample.positionMm - previous.positionMm);
      if (
        accumulatedDistance >= options.maxDistanceMm * (1.0 - 1.0e-12) ||
        accumulatedTime >= options.maxActiveSeconds * (1.0 - 1.0e-12))
      {
        flush(point);
      }
    }
  }
  flush(samples.back().positionMm);
  return result;
}
} // namespace deformation
