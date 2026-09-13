#include "image/ImageTimeAxis.h"

#include <algorithm>
#include <cctype>
#include <cmath>

namespace
{
std::string lowerAscii(std::string text)
{
  std::transform(text.begin(), text.end(), text.begin(), [](unsigned char ch) {
    return static_cast<char>(std::tolower(ch));
  });
  return text;
}

double secondsPerUnit(const std::string& units)
{
  const std::string lower = lowerAscii(units);
  if (lower == "s" || lower == "sec" || lower == "secs" || lower == "second" || lower == "seconds") {
    return 1.0;
  }
  if (lower == "ms" || lower == "msec" || lower == "millisecond" || lower == "milliseconds") {
    return 1.0e-3;
  }
  if (lower == "us" || lower == "usec" || lower == "microsecond" || lower == "microseconds") {
    return 1.0e-6;
  }
  if (lower == "ns" || lower == "nsec" || lower == "nanosecond" || lower == "nanoseconds") {
    return 1.0e-9;
  }

  // Unitless frame axes should still be useful for cine playback.
  return 1.0 / 10.0;
}
} // namespace

ImageTimeAxis::ImageTimeAxis(uint32_t numTimePoints, double origin, double spacing, std::string units)
  : m_units(std::move(units))
{
  const uint32_t count = std::max(1u, numTimePoints);
  m_timePoints.resize(count);
  for (uint32_t i = 0; i < count; ++i) {
    m_timePoints[i] = origin + static_cast<double>(i) * spacing;
  }
}

ImageTimeAxis::ImageTimeAxis(std::vector<double> timePoints, std::string units)
  : m_timePoints(std::move(timePoints)), m_units(std::move(units))
{
  if (m_timePoints.empty()) {
    m_timePoints.push_back(0.0);
  }
}

bool ImageTimeAxis::isTimeSeries() const
{
  return m_timePoints.size() > 1;
}

uint32_t ImageTimeAxis::numTimePoints() const
{
  return static_cast<uint32_t>(m_timePoints.size());
}

const std::string& ImageTimeAxis::units() const
{
  return m_units;
}

std::optional<double> ImageTimeAxis::spacing() const
{
  if (m_timePoints.size() < 2u) {
    return std::nullopt;
  }

  const double spacing = m_timePoints[1] - m_timePoints[0];
  constexpr double k_tolerance = 1.0e-9;
  for (std::size_t i = 2; i < m_timePoints.size(); ++i) {
    if (std::abs((m_timePoints[i] - m_timePoints[i - 1u]) - spacing) > k_tolerance) {
      return std::nullopt;
    }
  }

  return spacing;
}

double ImageTimeAxis::playbackFramePeriodSeconds(double playbackSpeed) const
{
  constexpr double k_minFramePeriodSeconds = 1.0 / 120.0;
  constexpr double k_maxFramePeriodSeconds = 5.0;
  const double safeSpeed = playbackSpeed > 0.0 ? playbackSpeed : 1.0;
  const double axisSpacing = spacing().value_or(1.0);
  const double period = std::abs(axisSpacing) * secondsPerUnit(m_units) / safeSpeed;
  if (!std::isfinite(period) || period <= 0.0) {
    return 1.0 / 10.0;
  }
  return std::clamp(period, k_minFramePeriodSeconds, k_maxFramePeriodSeconds);
}

double ImageTimeAxis::maxPlaybackSpeedForFrameRate(double maxFramesPerSecond) const
{
  const double axisSpacing = spacing().value_or(1.0);
  const double periodAtOneX = std::abs(axisSpacing) * secondsPerUnit(m_units);
  if (
    !std::isfinite(periodAtOneX) || periodAtOneX <= 0.0 || !std::isfinite(maxFramesPerSecond) ||
    maxFramesPerSecond <= 0.0)
  {
    return 1.0;
  }
  return maxFramesPerSecond * periodAtOneX;
}

std::optional<double> ImageTimeAxis::value(uint32_t timePoint) const
{
  if (timePoint >= m_timePoints.size()) {
    return std::nullopt;
  }
  return m_timePoints[timePoint];
}

uint32_t ImageTimeAxis::clamp(uint32_t timePoint) const
{
  return std::min(timePoint, numTimePoints() - 1u);
}

std::pair<std::size_t, std::size_t> separateComponentFrameAddress(
  uint32_t component,
  std::size_t spatialIndex,
  uint32_t timePoint,
  std::size_t spatialVoxelCount)
{
  return {component, static_cast<std::size_t>(timePoint) * spatialVoxelCount + spatialIndex};
}

std::pair<std::size_t, std::size_t> interleavedComponentFrameAddress(
  uint32_t component,
  std::size_t spatialIndex,
  uint32_t timePoint,
  std::size_t spatialVoxelCount,
  uint32_t numComponents)
{
  const std::size_t frameOffset = static_cast<std::size_t>(timePoint) * spatialVoxelCount * numComponents;
  return {0u, frameOffset + spatialIndex * numComponents + component};
}
