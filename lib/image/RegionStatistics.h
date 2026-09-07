#pragma once

#include <cstddef>
#include <cstdint>
#include <array>
#include <expected>
#include <optional>
#include <string>
#include <vector>

class Image;

enum class RegionValueKind : std::uint8_t
{
  Component,
  Magnitude,
  Minimum,
  Mean,
  Maximum,
  Luminance,
  ComplexReal,
  ComplexImaginary,
  ComplexMagnitude,
  ComplexPhase
};

struct RegionValueSelection
{
  RegionValueKind kind = RegionValueKind::Component;
  std::uint32_t component = 0;

  friend bool operator==(const RegionValueSelection&, const RegionValueSelection&) = default;
};

struct RegionStatistic
{
  std::int64_t label = 0;
  std::uint64_t elementCount = 0;
  std::uint64_t finiteValueCount = 0;
  std::uint64_t nonFiniteValueCount = 0;
  double physicalSize = 0.0;
  double minimum = 0.0;
  double mean = 0.0;
  double maximum = 0.0;
  double standardDeviation = 0.0;
  std::optional<double> firstQuartile;
  std::optional<double> median;
  std::optional<double> thirdQuartile;
};

struct RegionStatisticsOptions
{
  /** Compute exact quartiles. This temporarily retains the finite values for every region. */
  bool computeQuartiles = false;
};

struct RegionStatisticsResult
{
  std::vector<RegionStatistic> regions;
  bool isVolume = true;
  double elementPhysicalSize = 0.0;
};

struct RegionHistogram
{
  std::vector<double> binCenters;
  std::vector<double> counts;
  std::uint64_t finiteValueCount = 0;
  std::uint64_t nonFiniteValueCount = 0;
  double minimum = 0.0;
  double maximum = 0.0;
};

struct RegionHistogramOptions
{
  std::size_t binCount = 128;
  std::optional<std::array<double, 2>> valueRange;

  friend bool operator==(const RegionHistogramOptions&, const RegionHistogramOptions&) = default;
};

enum class RegionStatisticsError : std::uint8_t
{
  MissingPixelData,
  MismatchedDimensions,
  MismatchedSpatialGeometry,
  InvalidSegmentation,
  InvalidValueSelection,
  InvalidTimePoint,
  InvalidBinCount,
  InvalidValueRange
};

/** Human-readable description of a region-statistics calculation error. */
std::string regionStatisticsErrorMessage(RegionStatisticsError error);

/** Compute per-label statistics from an intensity image and matching segmentation. */
std::expected<RegionStatisticsResult, RegionStatisticsError> computeRegionStatistics(
  const Image& image,
  const Image& segmentation,
  const RegionValueSelection& valueSelection,
  std::uint32_t timePoint = 0,
  const RegionStatisticsOptions& options = {});

/** Compute a raw-value histogram for either a label region or the whole image. */
std::expected<RegionHistogram, RegionStatisticsError> computeRegionHistogram(
  const Image& image,
  const Image& segmentation,
  const RegionValueSelection& valueSelection,
  std::uint32_t timePoint,
  std::optional<std::int64_t> label,
  const RegionHistogramOptions& options);

/** Human-readable label for an image value selection. */
std::string regionValueSelectionName(const RegionValueSelection& selection);
