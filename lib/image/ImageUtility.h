#pragma once

#include "common/HistogramSettings.h"
#include "common/Types.h"
#include "image/ImageHeader.h"
#include "image/ImageTypes.h"
#include "image/external/TDigest.h"

#include <glm/vec3.hpp>

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <utility>
#include <vector>

class Image;

/**
 * @brief Get the final file-name component of a path.
 * @param filePath Input path string.
 * @param withExtension When true, keep the file extension; otherwise strip it.
 */
std::string getFileName(const std::string& filePath, bool withExtension = false);

/**
 * @brief Read image metadata without loading pixel buffers.
 * @param fileName Image file to inspect.
 * @param imageRep Semantic image representation requested by the caller.
 * @param bufferType Desired multi-component buffer layout.
 * @return Header on success, or std::nullopt when the file cannot be read as an image.
 */
std::optional<ImageHeader> readImageHeaderOnly(
  const std::filesystem::path& fileName,
  ImageRepresentation imageRep,
  MultiComponentBufferType bufferType);

/**
 * @brief Get the range of values that can be held in components of a given type.
 * Only for components supported by Entropy.
 * @param componentType Component type to query.
 * @return Inclusive numeric range, or {0, 0} for unsupported/undefined types.
 */
std::pair<double, double> componentRange(const ComponentType& componentType);

/**
 * @brief Compute the axis-aligned world-space bounds of an image.
 * @return Pair of minimum and maximum world-space corners.
 */
std::pair<glm::vec3, glm::vec3> computeWorldMinMaxCornersOfImage(const Image& image);

/// @brief Compute exact component statistics from sorted image values.
std::vector<ComponentStats> computeImageStatisticsOnSortedValues(const Image& image);
/// @brief Compute online component statistics without sorting image values.
std::vector<OnlineStats> computeImageStatisticsOnUnsortedValues(const Image& image);

/// @brief Compute a T-digest approximation for each image component.
std::vector<tdigest::TDigest> computeTDigests(const Image& image);

/**
 * @brief Nudge an attempted quantile away from the current value when it maps to the same value.
 *
 * This supports window/level editing by avoiding no-op quantile moves on flat or discrete data.
 */
double bumpQuantile(
  const Image& image,
  uint32_t comp,
  double currentQuantile,
  double attemptedQuantile,
  double currentValue,
  bool usingExactQuantiles);

/**
 * @brief Compute a histogram bin count using the requested rule.
 * @return Bin count, or std::nullopt when the method cannot be computed for the supplied data.
 */
std::optional<std::size_t>
computeNumHistogramBins(const NumBinsComputationMethod& method, std::size_t numPixels, ComponentStats stats);
