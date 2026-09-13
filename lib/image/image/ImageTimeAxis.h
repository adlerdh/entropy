#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

/**
 * @brief Time-axis metadata for an image whose pixels vary over multiple frames.
 */
class ImageTimeAxis
{
public:
  /**
   * @brief Create a single-frame time axis.
   */
  ImageTimeAxis() = default;

  /**
   * @brief Create a regularly sampled time axis.
   * @param numTimePoints Number of frames along the time axis.
   * @param origin Time value of the first frame.
   * @param spacing Time spacing between adjacent frames.
   * @param units Display units for time values.
   */
  ImageTimeAxis(uint32_t numTimePoints, double origin, double spacing, std::string units);

  /**
   * @brief Create an explicitly sampled time axis.
   * @param timePoints Time value for each frame.
   * @param units Display units for time values.
   */
  ImageTimeAxis(std::vector<double> timePoints, std::string units);

  /**
   * @brief Return true when the image has more than one time point.
   */
  bool isTimeSeries() const;

  /**
   * @brief Get the number of time points.
   */
  uint32_t numTimePoints() const;

  /**
   * @brief Get the display units for time values.
   */
  const std::string& units() const;

  /**
   * @brief Get the regular spacing between time points.
   * @return Time spacing, or std::nullopt when fewer than two time points exist or spacing is not regular.
   */
  std::optional<double> spacing() const;

  /**
   * @brief Estimate the playback period for one frame.
   * @param playbackSpeed Playback speed multiplier. Values less than or equal to zero are treated as 1.0.
   * @return Frame period in seconds.
   */
  double playbackFramePeriodSeconds(double playbackSpeed = 1.0) const;

  /**
   * @brief Compute the highest playback multiplier that does not exceed a requested frame rate.
   * @param maxFramesPerSecond Maximum effective playback rate in frames per second.
   * @return Playback multiplier for the requested frame-rate limit.
   */
  double maxPlaybackSpeedForFrameRate(double maxFramesPerSecond) const;

  /**
   * @brief Get the time value at a frame index.
   * @param timePoint Time-point index.
   * @return Time value, or std::nullopt when \p timePoint is out of range.
   */
  std::optional<double> value(uint32_t timePoint) const;

  /**
   * @brief Clamp a frame index to the valid range.
   * @param timePoint Requested time-point index.
   * @return Valid time-point index.
   */
  uint32_t clamp(uint32_t timePoint) const;

private:
  std::vector<double> m_timePoints{0.0};
  std::string m_units{"frame"};
};

/**
 * @brief Map a logical component, spatial index, and time point to a separate-component buffer offset.
 * @param component Logical component index.
 * @param spatialIndex Linear spatial voxel index.
 * @param timePoint Time-point index.
 * @param spatialVoxelCount Number of voxels in one spatial frame.
 * @return Buffer index and element offset.
 */
std::pair<std::size_t, std::size_t> separateComponentFrameAddress(
  uint32_t component,
  std::size_t spatialIndex,
  uint32_t timePoint,
  std::size_t spatialVoxelCount);

/**
 * @brief Map a logical component, spatial index, and time point to an interleaved buffer offset.
 * @param component Logical component index.
 * @param spatialIndex Linear spatial voxel index.
 * @param timePoint Time-point index.
 * @param spatialVoxelCount Number of voxels in one spatial frame.
 * @param numComponents Number of components per voxel.
 * @return Buffer index and element offset.
 */
std::pair<std::size_t, std::size_t> interleavedComponentFrameAddress(
  uint32_t component,
  std::size_t spatialIndex,
  uint32_t timePoint,
  std::size_t spatialVoxelCount,
  uint32_t numComponents);
