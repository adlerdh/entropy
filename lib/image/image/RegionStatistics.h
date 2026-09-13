#pragma once

#include <cstddef>
#include <cstdint>
#include <array>
#include <expected>
#include <optional>
#include <string>
#include <vector>

class Image;

/// @brief Scalar value derived from each image pixel for regional analysis
enum class RegionValueKind : std::uint8_t
{
  Component,        //!< One selected image component
  Magnitude,        //!< Euclidean magnitude across all components
  Minimum,          //!< Minimum value across all components
  Mean,             //!< Arithmetic mean across all components
  Maximum,          //!< Maximum value across all components
  Luminance,        //!< Linear luminance derived from the first three components
  ComplexReal,      //!< Real part of a complex pixel
  ComplexImaginary, //!< Imaginary part of a complex pixel
  ComplexMagnitude, //!< Magnitude of a complex pixel
  ComplexPhase      //!< Phase of a complex pixel in radians
};

/// @brief Selection of the scalar image value used to compute statistics
struct RegionValueSelection
{
  RegionValueKind kind = RegionValueKind::Component; //!< Scalar projection to evaluate
  std::uint32_t component = 0;                       //!< Component index used when kind is Component

  friend bool operator==(const RegionValueSelection&, const RegionValueSelection&) = default;
};

/// @brief Intensity and physical-size statistics for one segmentation label
struct RegionStatistic
{
  std::int64_t label = 0;                //!< Integer segmentation label
  std::uint64_t elementCount = 0;        //!< Number of pixels or voxels assigned to the label
  std::uint64_t finiteValueCount = 0;    //!< Elements having finite selected image values
  std::uint64_t nonFiniteValueCount = 0; //!< Elements having non-finite selected image values
  double physicalSize = 0.0;             //!< Region area in mm^2 or volume in mm^3
  double minimum = 0.0;                  //!< Minimum finite selected value
  double mean = 0.0;                     //!< Mean finite selected value
  double maximum = 0.0;                  //!< Maximum finite selected value
  double standardDeviation = 0.0;        //!< Population standard deviation of finite selected values
  std::optional<double> firstQuartile;   //!< 25th percentile when quartiles were requested
  std::optional<double> median;          //!< 50th percentile when quartiles were requested
  std::optional<double> thirdQuartile;   //!< 75th percentile when quartiles were requested
};

/// @brief Options controlling the per-label statistics calculation
struct RegionStatisticsOptions
{
  /// Compute exact quartiles, temporarily retaining the finite values for every region
  bool computeQuartiles = false;
};

/// @brief Per-label statistics and physical sampling information for a segmentation
struct RegionStatisticsResult
{
  std::vector<RegionStatistic> regions; //!< Statistics ordered by ascending label value
  bool isVolume = true;                 //!< True for voxel volumes and false for pixel areas
  double elementPhysicalSize = 0.0;     //!< Area in mm^2 or volume in mm^3 of one element
};

/// @brief Raw histogram of selected image values within an optional label region
struct RegionHistogram
{
  std::vector<double> binCenters;        //!< Center image value of each bin
  std::vector<double> counts;            //!< Number of finite values assigned to each bin
  std::uint64_t finiteValueCount = 0;    //!< Finite values included in the histogram range
  std::uint64_t nonFiniteValueCount = 0; //!< Selected elements having non-finite values
  double minimum = 0.0;                  //!< Lower histogram bound
  double maximum = 0.0;                  //!< Upper histogram bound
};

/// @brief Options controlling region histogram binning and value limits
struct RegionHistogramOptions
{
  std::size_t binCount = 128;                      //!< Number of equal-width histogram bins
  std::optional<std::array<double, 2>> valueRange; //!< Optional inclusive minimum and maximum values

  friend bool operator==(const RegionHistogramOptions&, const RegionHistogramOptions&) = default;
};

/// @brief Error categories returned by region-statistics and histogram calculations
enum class RegionStatisticsError : std::uint8_t
{
  MissingPixelData,          //!< The image or segmentation has no loaded pixel data
  MismatchedDimensions,      //!< Image and segmentation pixel dimensions differ
  MismatchedSpatialGeometry, //!< Image and segmentation physical geometry differs
  InvalidSegmentation,       //!< Segmentation does not contain one scalar label component
  InvalidValueSelection,     //!< Selected scalar projection is incompatible with the image
  InvalidTimePoint,          //!< Requested image time point is out of range
  InvalidBinCount,           //!< Histogram bin count is zero
  InvalidValueRange          //!< Histogram range is non-finite or not increasing
};

/// @brief Return a human-readable description of a region-statistics calculation error
/// @param error Error category to describe
/// @return User-facing error message
std::string regionStatisticsErrorMessage(RegionStatisticsError error);

/// @brief Compute per-label statistics from an intensity image and matching segmentation
/// @param image Image providing the values to analyze
/// @param segmentation Single-component label image with matching dimensions and physical geometry
/// @param valueSelection Scalar value derived from each image pixel
/// @param timePoint Image time point to analyze, while segmentation labels are read from their first time point
/// @param options Optional quartile calculation settings
/// @return Statistics for every label present, or a validation error
std::expected<RegionStatisticsResult, RegionStatisticsError> computeRegionStatistics(
  const Image& image,
  const Image& segmentation,
  const RegionValueSelection& valueSelection,
  std::uint32_t timePoint = 0,
  const RegionStatisticsOptions& options = {});

/// @brief Compute a raw-value histogram for either a label region or the whole image
/// @param image Image providing the values to bin
/// @param segmentation Single-component label image with matching dimensions and physical geometry
/// @param valueSelection Scalar value derived from each image pixel
/// @param timePoint Image time point to analyze
/// @param label Label to include, or std::nullopt to include the whole image
/// @param options Histogram bin count and optional explicit value range
/// @return Raw histogram data, or a validation error
std::expected<RegionHistogram, RegionStatisticsError> computeRegionHistogram(
  const Image& image,
  const Image& segmentation,
  const RegionValueSelection& valueSelection,
  std::uint32_t timePoint,
  std::optional<std::int64_t> label,
  const RegionHistogramOptions& options);

/// @brief Return a human-readable label for an image value selection
/// @param selection Scalar value selection to describe
/// @return User-facing selection name
std::string regionValueSelectionName(const RegionValueSelection& selection);
