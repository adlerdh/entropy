#pragma once

#include "common/Types.h"
#include "image/Image.h"

#include "TDigest.h"

#include <glm/vec3.hpp>

#include <itkCommonEnums.h>
#include <itkImage.h>
#include <itkImageIOBase.h>

#include <string>
#include <utility>

/**
 * @brief Get file name from a path with or without extension
 */
std::string getFileName(const std::string& filePath, bool withExtension = false);

PixelType fromItkPixelType(const itk::IOPixelEnum& pixelType);

ComponentType fromItkComponentType(const itk::IOComponentEnum& componentType);

itk::IOComponentEnum toItkComponentType(const ComponentType& componentType);

std::pair<itk::CommonEnums::IOComponent, std::string> sniffComponentType(const char* fileName);

typename itk::ImageIOBase::Pointer createStandardImageIo(const char* fileName);

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

double bumpQuantile(const Image& image, uint32_t comp,
                    double currentQuantile, double attemptedQuantile, double currentValue,
                    bool usingExactQuantiles);

std::optional<std::size_t> computeNumHistogramBins(
  const NumBinsComputationMethod& method, std::size_t numPixels, ComponentStats stats
);
