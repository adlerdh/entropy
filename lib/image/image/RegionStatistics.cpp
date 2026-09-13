#include "image/RegionStatistics.h"

#include "image/Image.h"

#include <glm/geometric.hpp>
#include <glm/gtc/matrix_access.hpp>

#include <algorithm>
#include <cmath>
#include <expected>
#include <limits>
#include <map>

namespace
{
struct RunningStatistic
{
  std::uint64_t elementCount = 0;
  std::uint64_t finiteCount = 0;
  std::uint64_t nonFiniteCount = 0;
  double minimum = std::numeric_limits<double>::infinity();
  double mean = 0.0;
  double maximum = -std::numeric_limits<double>::infinity();
  double m2 = 0.0;

  void add(double value)
  {
    ++elementCount;
    if (!std::isfinite(value)) {
      ++nonFiniteCount;
      return;
    }

    ++finiteCount;
    minimum = std::min(minimum, value);
    maximum = std::max(maximum, value);
    const double delta = value - mean;
    mean += delta / static_cast<double>(finiteCount);
    m2 += delta * (value - mean);
  }
};

double rawValue(const void* values, ComponentType type, std::size_t index)
{
  switch (type) {
    case ComponentType::Int8:
      return static_cast<const std::int8_t*>(values)[index];
    case ComponentType::UInt8:
      return static_cast<const std::uint8_t*>(values)[index];
    case ComponentType::Int16:
      return static_cast<const std::int16_t*>(values)[index];
    case ComponentType::UInt16:
      return static_cast<const std::uint16_t*>(values)[index];
    case ComponentType::Int32:
      return static_cast<const std::int32_t*>(values)[index];
    case ComponentType::UInt32:
      return static_cast<const std::uint32_t*>(values)[index];
    case ComponentType::Float32:
      return static_cast<const float*>(values)[index];
    default:
      return std::numeric_limits<double>::quiet_NaN();
  }
}

class PixelAccessor
{
public:
  PixelAccessor(const Image& image, std::uint32_t timePoint)
    : m_type(image.header().memoryComponentType())
    , m_componentCount(image.header().numComponentsPerPixel())
    , m_interleaved(MultiComponentBufferType::InterleavedImage == image.bufferType())
  {
    const std::uint32_t bufferCount = m_interleaved ? 1u : m_componentCount;
    m_buffers.reserve(bufferCount);
    for (std::uint32_t component = 0; component < bufferCount; ++component) {
      m_buffers.push_back(image.bufferAsVoid(component, timePoint));
    }
  }

  [[nodiscard]] std::optional<double> value(std::uint32_t component, std::size_t index) const
  {
    if (component >= m_componentCount) return std::nullopt;
    const void* values = m_interleaved ? m_buffers.front() : m_buffers[component];
    if (!values) return std::nullopt;
    const std::size_t offset = m_interleaved ? index * m_componentCount + component : index;
    return rawValue(values, m_type, offset);
  }

  [[nodiscard]] std::uint32_t componentCount() const
  {
    return m_componentCount;
  }

private:
  ComponentType m_type = ComponentType::Undefined;
  std::uint32_t m_componentCount = 0;
  bool m_interleaved = false;
  std::vector<const void*> m_buffers;
};

std::optional<double>
selectedValue(const PixelAccessor& image, const RegionValueSelection& selection, std::size_t index)
{
  const std::uint32_t componentCount = image.componentCount();
  const auto component = [&]() {
    return image.value(selection.component, index);
  };

  switch (selection.kind) {
    case RegionValueKind::Component:
      return component();
    case RegionValueKind::ComplexReal:
      return image.value(0, index);
    case RegionValueKind::ComplexImaginary:
      return image.value(1, index);
    case RegionValueKind::ComplexMagnitude:
    case RegionValueKind::ComplexPhase: {
      const auto real = image.value(0, index);
      const auto imaginary = image.value(1, index);
      if (!real || !imaginary) return std::nullopt;
      if (RegionValueKind::ComplexPhase == selection.kind) return std::atan2(*imaginary, *real);
      return std::hypot(*real, *imaginary);
    }
    case RegionValueKind::Luminance: {
      if (componentCount < 3) return std::nullopt;
      const auto red = image.value(0, index);
      const auto green = image.value(1, index);
      const auto blue = image.value(2, index);
      if (!red || !green || !blue) return std::nullopt;
      return 0.2126 * *red + 0.7152 * *green + 0.0722 * *blue;
    }
    case RegionValueKind::Magnitude:
    case RegionValueKind::Minimum:
    case RegionValueKind::Mean:
    case RegionValueKind::Maximum: {
      if (0 == componentCount) return std::nullopt;
      double aggregate = RegionValueKind::Minimum == selection.kind   ? std::numeric_limits<double>::infinity()
                         : RegionValueKind::Maximum == selection.kind ? -std::numeric_limits<double>::infinity()
                                                                      : 0.0;
      for (std::uint32_t c = 0; c < componentCount; ++c) {
        const auto value = image.value(c, index);
        if (!value) return std::nullopt;
        if (RegionValueKind::Magnitude == selection.kind)
          aggregate += *value * *value;
        else if (RegionValueKind::Minimum == selection.kind)
          aggregate = std::min(aggregate, *value);
        else if (RegionValueKind::Maximum == selection.kind)
          aggregate = std::max(aggregate, *value);
        else
          aggregate += *value;
      }
      if (RegionValueKind::Magnitude == selection.kind) return std::sqrt(aggregate);
      if (RegionValueKind::Mean == selection.kind) return aggregate / static_cast<double>(componentCount);
      return aggregate;
    }
  }
  return std::nullopt;
}

std::optional<std::int64_t> labelValue(const PixelAccessor& segmentation, std::size_t index)
{
  const auto value = segmentation.value(0, index);
  if (!value || !std::isfinite(*value)) return std::nullopt;
  return static_cast<std::int64_t>(std::llround(*value));
}

std::pair<bool, double> physicalElementSize(const Image& segmentation)
{
  const auto& spacing = segmentation.header().spacing();
  const auto& directions = segmentation.header().directions();
  const glm::dvec3 axis0 = glm::dvec3(glm::column(directions, 0)) * static_cast<double>(spacing.x);
  const glm::dvec3 axis1 = glm::dvec3(glm::column(directions, 1)) * static_cast<double>(spacing.y);
  const bool isVolume = segmentation.header().numSpatialDimensions() >= 3u;
  if (!isVolume) return {false, glm::length(glm::cross(axis0, axis1))};
  const glm::dvec3 axis2 = glm::dvec3(glm::column(directions, 2)) * static_cast<double>(spacing.z);
  return {true, std::abs(glm::dot(axis0, glm::cross(axis1, axis2)))};
}

bool nearlyEqual(double left, double right)
{
  constexpr double absoluteTolerance = 1e-5;
  constexpr double relativeTolerance = 1e-5;
  return std::abs(left - right) <= absoluteTolerance + relativeTolerance * std::max(std::abs(left), std::abs(right));
}

template<glm::length_t Length, typename T, glm::qualifier Qualifier>
bool nearlyEqual(const glm::vec<Length, T, Qualifier>& left, const glm::vec<Length, T, Qualifier>& right)
{
  for (glm::length_t index = 0; index < Length; ++index) {
    if (!nearlyEqual(left[index], right[index])) return false;
  }
  return true;
}

bool nearlyEqual(const glm::mat3& left, const glm::mat3& right)
{
  for (glm::length_t column = 0; column < 3; ++column) {
    if (!nearlyEqual(left[column], right[column])) return false;
  }
  return true;
}

bool validSelection(const Image& image, const RegionValueSelection& selection)
{
  const std::uint32_t components = image.header().numComponentsPerPixel();
  switch (selection.kind) {
    case RegionValueKind::Component:
      return selection.component < components;
    case RegionValueKind::Luminance:
      return components >= 3u;
    case RegionValueKind::ComplexReal:
    case RegionValueKind::ComplexImaginary:
    case RegionValueKind::ComplexMagnitude:
    case RegionValueKind::ComplexPhase:
      return PixelType::Complex == image.header().pixelType() && components >= 2u;
    case RegionValueKind::Magnitude:
    case RegionValueKind::Minimum:
    case RegionValueKind::Mean:
    case RegionValueKind::Maximum:
      return components > 0u;
  }
  return false;
}

std::optional<RegionStatisticsError> validateInputs(
  const Image& image,
  const Image& segmentation,
  const RegionValueSelection& selection,
  std::uint32_t timePoint)
{
  if (!image.hasPixelData() || !segmentation.hasPixelData()) return RegionStatisticsError::MissingPixelData;
  if (1u != segmentation.header().numComponentsPerPixel()) return RegionStatisticsError::InvalidSegmentation;
  if (image.header().pixelDimensions() != segmentation.header().pixelDimensions()) {
    return RegionStatisticsError::MismatchedDimensions;
  }
  if (
    image.header().numSpatialDimensions() != segmentation.header().numSpatialDimensions() ||
    !nearlyEqual(image.header().origin(), segmentation.header().origin()) ||
    !nearlyEqual(image.header().spacing(), segmentation.header().spacing()) ||
    !nearlyEqual(image.header().directions(), segmentation.header().directions()))
  {
    return RegionStatisticsError::MismatchedSpatialGeometry;
  }
  if (!validSelection(image, selection)) return RegionStatisticsError::InvalidValueSelection;
  if (timePoint >= image.timeAxis().numTimePoints()) return RegionStatisticsError::InvalidTimePoint;
  return std::nullopt;
}
} // namespace

std::expected<RegionStatisticsResult, RegionStatisticsError> computeRegionStatistics(
  const Image& image,
  const Image& segmentation,
  const RegionValueSelection& valueSelection,
  std::uint32_t timePoint,
  const RegionStatisticsOptions& options)
{
  if (const auto error = validateInputs(image, segmentation, valueSelection, timePoint)) {
    return std::unexpected(*error);
  }

  const PixelAccessor imageValues(image, timePoint);
  const PixelAccessor segmentationValues(segmentation, 0);
  std::map<std::int64_t, RunningStatistic> running;
  std::map<std::int64_t, std::vector<double>> finiteValues;
  for (std::size_t index = 0; index < image.header().numPixels(); ++index) {
    const auto label = labelValue(segmentationValues, index);
    const auto value = selectedValue(imageValues, valueSelection, index);
    if (!label || !value) continue;
    running[*label].add(*value);
    if (options.computeQuartiles && std::isfinite(*value)) finiteValues[*label].push_back(*value);
  }

  const auto [isVolume, elementSize] = physicalElementSize(segmentation);
  RegionStatisticsResult result;
  result.isVolume = isVolume;
  result.elementPhysicalSize = elementSize;
  result.regions.reserve(running.size());
  for (const auto& [label, accumulator] : running) {
    RegionStatistic statistic;
    statistic.label = label;
    statistic.elementCount = accumulator.elementCount;
    statistic.finiteValueCount = accumulator.finiteCount;
    statistic.nonFiniteValueCount = accumulator.nonFiniteCount;
    statistic.physicalSize = static_cast<double>(accumulator.elementCount) * elementSize;
    if (accumulator.finiteCount > 0) {
      statistic.minimum = accumulator.minimum;
      statistic.mean = accumulator.mean;
      statistic.maximum = accumulator.maximum;
      statistic.standardDeviation = std::sqrt(accumulator.m2 / static_cast<double>(accumulator.finiteCount));
      if (options.computeQuartiles) {
        auto& values = finiteValues[label];
        std::sort(values.begin(), values.end());
        const auto quantile = [&values](double probability) {
          const double position = probability * static_cast<double>(values.size() - 1u);
          const auto lower = static_cast<std::size_t>(std::floor(position));
          const auto upper = static_cast<std::size_t>(std::ceil(position));
          const double fraction = position - static_cast<double>(lower);
          return values[lower] + fraction * (values[upper] - values[lower]);
        };
        statistic.firstQuartile = quantile(0.25);
        statistic.median = quantile(0.50);
        statistic.thirdQuartile = quantile(0.75);
      }
    }
    result.regions.push_back(statistic);
  }
  return result;
}

std::expected<RegionHistogram, RegionStatisticsError> computeRegionHistogram(
  const Image& image,
  const Image& segmentation,
  const RegionValueSelection& valueSelection,
  std::uint32_t timePoint,
  std::optional<std::int64_t> label,
  const RegionHistogramOptions& options)
{
  if (const auto error = validateInputs(image, segmentation, valueSelection, timePoint)) {
    return std::unexpected(*error);
  }
  if (0 == options.binCount) return std::unexpected(RegionStatisticsError::InvalidBinCount);
  if (
    options.valueRange && (!std::isfinite((*options.valueRange)[0]) || !std::isfinite((*options.valueRange)[1]) ||
                           (*options.valueRange)[0] >= (*options.valueRange)[1]))
  {
    return std::unexpected(RegionStatisticsError::InvalidValueRange);
  }

  const PixelAccessor imageValues(image, timePoint);
  const PixelAccessor segmentationValues(segmentation, 0);
  RegionHistogram result;
  result.minimum = options.valueRange ? (*options.valueRange)[0] : std::numeric_limits<double>::infinity();
  result.maximum = options.valueRange ? (*options.valueRange)[1] : -std::numeric_limits<double>::infinity();
  for (std::size_t index = 0; index < image.header().numPixels(); ++index) {
    if (label && labelValue(segmentationValues, index) != label) continue;
    const auto value = selectedValue(imageValues, valueSelection, index);
    if (!value || !std::isfinite(*value)) {
      ++result.nonFiniteValueCount;
      continue;
    }
    if (options.valueRange && (*value < result.minimum || *value > result.maximum)) continue;
    ++result.finiteValueCount;
    if (!options.valueRange) {
      result.minimum = std::min(result.minimum, *value);
      result.maximum = std::max(result.maximum, *value);
    }
  }
  if (0 == result.finiteValueCount) {
    result.minimum = result.maximum = 0.0;
    return result;
  }

  result.counts.assign(options.binCount, 0.0);
  result.binCenters.resize(options.binCount);
  const double width =
    result.maximum > result.minimum ? (result.maximum - result.minimum) / static_cast<double>(options.binCount) : 1.0;
  for (std::size_t bin = 0; bin < options.binCount; ++bin) {
    result.binCenters[bin] = result.minimum + (static_cast<double>(bin) + 0.5) * width;
  }
  for (std::size_t index = 0; index < image.header().numPixels(); ++index) {
    if (label && labelValue(segmentationValues, index) != label) continue;
    const auto value = selectedValue(imageValues, valueSelection, index);
    if (!value || !std::isfinite(*value)) continue;
    if (options.valueRange && (*value < result.minimum || *value > result.maximum)) continue;
    const std::size_t bin =
      result.maximum == result.minimum
        ? 0u
        : std::min(options.binCount - 1u, static_cast<std::size_t>((*value - result.minimum) / width));
    result.counts[bin] += 1.0;
  }
  return result;
}

std::string regionStatisticsErrorMessage(RegionStatisticsError error)
{
  switch (error) {
    case RegionStatisticsError::MissingPixelData:
      return "Image or segmentation pixel data is unavailable";
    case RegionStatisticsError::MismatchedDimensions:
      return "Image and segmentation pixel dimensions do not match";
    case RegionStatisticsError::MismatchedSpatialGeometry:
      return "Image and segmentation origin, spacing, or directions do not match";
    case RegionStatisticsError::InvalidSegmentation:
      return "The segmentation must contain exactly one component per pixel";
    case RegionStatisticsError::InvalidValueSelection:
      return "The selected image value is unavailable";
    case RegionStatisticsError::InvalidTimePoint:
      return "The selected image time point is unavailable";
    case RegionStatisticsError::InvalidBinCount:
      return "Histogram bin count must be greater than zero";
    case RegionStatisticsError::InvalidValueRange:
      return "Histogram image-value range must have a finite minimum below its maximum";
  }
  return "Region statistics could not be computed";
}

std::string regionValueSelectionName(const RegionValueSelection& selection)
{
  switch (selection.kind) {
    case RegionValueKind::Component:
      return "Component " + std::to_string(selection.component + 1u);
    case RegionValueKind::Magnitude:
      return "Magnitude";
    case RegionValueKind::Minimum:
      return "Component minimum";
    case RegionValueKind::Mean:
      return "Component mean";
    case RegionValueKind::Maximum:
      return "Component maximum";
    case RegionValueKind::Luminance:
      return "Luminance";
    case RegionValueKind::ComplexReal:
      return "Real";
    case RegionValueKind::ComplexImaginary:
      return "Imaginary";
    case RegionValueKind::ComplexMagnitude:
      return "Magnitude";
    case RegionValueKind::ComplexPhase:
      return "Phase (radians)";
  }
  return "Unknown";
}
