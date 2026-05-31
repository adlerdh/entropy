#pragma once

#include "Types.h"
#include "Image.h"

#include "TDigest.h"

#include <glm/vec3.hpp>

#include <optional>
#include <string>
#include <utility>

/**
 * @brief Get file name from a path with or without extension
 */
std::string getFileName(const std::string& filePath, bool withExtension = false);

std::optional<ImageHeader> readImageHeaderOnly(
  const fs::path& fileName,
  Image::ImageRepresentation imageRep,
  Image::MultiComponentBufferType bufferType);

/**
 * @brief Get the range of values that can be held in components of a given type.
 * Only for components supported by Entropy.
 *
 * @param componentType
 * @return
 */
std::pair<double, double> componentRange(const ComponentType& componentType);

std::pair<glm::vec3, glm::vec3> computeWorldMinMaxCornersOfImage(const Image& image);

std::vector<ComponentStats> computeImageStatisticsOnSortedValues(const Image& image);
std::vector<OnlineStats> computeImageStatisticsOnUnsortedValues(const Image& image);

/// @brief Compute a T-digest for each image component
std::vector<tdigest::TDigest> computeTDigests(const Image& image);

double bumpQuantile(
  const Image& image,
  uint32_t comp,
  double currentQuantile,
  double attemptedQuantile,
  double currentValue,
  bool usingExactQuantiles);

std::optional<std::size_t>
computeNumHistogramBins(const NumBinsComputationMethod& method, std::size_t numPixels, ComponentStats stats);
