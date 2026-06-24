#include "image/ImageDerivedData.h"
#include "internal/ImageUtility.tpp"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <cmath>
#include <format>
#include <limits>
#include <numbers>
#include <optional>
#include <string>
#include <utility>

std::optional<ComponentProjectionMode> componentProjectionFromRenderMode(ComponentRenderMode mode)
{
  switch (mode) {
    case ComponentRenderMode::Minimum:
      return ComponentProjectionMode::Minimum;
    case ComponentRenderMode::Mean:
      return ComponentProjectionMode::Mean;
    case ComponentRenderMode::Maximum:
      return ComponentProjectionMode::Maximum;
    case ComponentRenderMode::Magnitude:
      return ComponentProjectionMode::Magnitude;
    case ComponentRenderMode::ComplexPhase:
      return ComponentProjectionMode::ComplexPhaseSignedRadians;
    case ComponentRenderMode::SingleComponent:
    case ComponentRenderMode::Color:
    case ComponentRenderMode::ComplexReal:
    case ComponentRenderMode::ComplexImaginary:
      return std::nullopt;
  }

  return std::nullopt;
}

std::optional<ComponentProjectionMode> componentProjectionForImage(const Image& image)
{
  if (ComponentRenderMode::ComplexPhase == image.settings().componentRenderMode()) {
    return complexPhaseProjectionMode(image.settings().complexPhaseRange(), image.settings().complexPhaseUnit());
  }

  return componentProjectionFromRenderMode(image.settings().componentRenderMode());
}

ComponentProjectionMode complexPhaseProjectionMode(ComplexPhaseRange range, ComplexPhaseUnit unit)
{
  if (ComplexPhaseUnit::Degrees == unit) {
    return ComplexPhaseRange::Unsigned == range ? ComponentProjectionMode::ComplexPhaseUnsignedDegrees
                                                : ComponentProjectionMode::ComplexPhaseSignedDegrees;
  }

  return ComplexPhaseRange::Unsigned == range ? ComponentProjectionMode::ComplexPhaseUnsignedRadians
                                              : ComponentProjectionMode::ComplexPhaseSignedRadians;
}

std::string componentProjectionModeName(ComponentProjectionMode mode)
{
  switch (mode) {
    case ComponentProjectionMode::Minimum:
      return "Minimum";
    case ComponentProjectionMode::Mean:
      return "Mean";
    case ComponentProjectionMode::Maximum:
      return "Maximum";
    case ComponentProjectionMode::Magnitude:
      return "Magnitude";
    case ComponentProjectionMode::ComplexPhaseSignedRadians:
      return "Phase [-pi, pi]";
    case ComponentProjectionMode::ComplexPhaseUnsignedRadians:
      return "Phase [0, 2pi]";
    case ComponentProjectionMode::ComplexPhaseSignedDegrees:
      return "Phase [-180, 180]";
    case ComponentProjectionMode::ComplexPhaseUnsignedDegrees:
      return "Phase [0, 360]";
  }

  return "Unknown";
}

bool isScalarComponentProjection(ComponentProjectionMode mode)
{
  switch (mode) {
    case ComponentProjectionMode::Minimum:
    case ComponentProjectionMode::Mean:
    case ComponentProjectionMode::Maximum:
    case ComponentProjectionMode::Magnitude:
    case ComponentProjectionMode::ComplexPhaseSignedRadians:
    case ComponentProjectionMode::ComplexPhaseUnsignedRadians:
    case ComponentProjectionMode::ComplexPhaseSignedDegrees:
    case ComponentProjectionMode::ComplexPhaseUnsignedDegrees:
      return true;
  }

  return false;
}

bool isComplexValuedImage(const Image& image)
{
  return PixelType::Complex == image.header().pixelType() && 2u == image.header().numComponentsPerPixel();
}

double complexPhaseValue(double real, double imaginary, ComplexPhaseRange range, ComplexPhaseUnit unit)
{
  constexpr double twoPi = 2.0 * std::numbers::pi;
  double phase = std::atan2(imaginary, real);

  if (ComplexPhaseRange::Unsigned == range && phase < 0.0) {
    phase += twoPi;
  }

  if (ComplexPhaseUnit::Degrees == unit) {
    phase *= 180.0 / std::numbers::pi;
  }

  return phase;
}

std::string complexComponentLabel(uint32_t component, uint32_t componentMax)
{
  std::string label = std::to_string(component) + " of " + std::to_string(componentMax);
  if (0u == component) {
    label += " (real)";
  }
  else if (1u == component) {
    label += " (imaginary)";
  }
  return label;
}

std::vector<ComponentImageResult> createNoiseEstimateImages(const Image& image, uint32_t radius)
{
  using TItkImageComp = float;

  std::vector<ComponentImageResult> results;
  results.reserve(image.header().numComponentsPerPixel());

  for (uint32_t comp = 0; comp < image.header().numComponentsPerPixel(); ++comp) {
    const auto compImage = createItkImageFromImageComponent<TItkImageComp>(image, comp);
    const auto noiseEstimateItkImage = computeNoiseEstimate<TItkImageComp>(compImage, radius);
    if (!noiseEstimateItkImage) {
      spdlog::warn("Unable to create noise estimate for component {}", comp);
      continue;
    }

    const std::string displayName =
      std::string("Noise estimate for comp ") + std::to_string(comp) + " of '" + image.settings().displayName() + "'";

    results.push_back(
      ComponentImageResult{comp, createImageFromItkImage<TItkImageComp>(noiseEstimateItkImage, displayName)});
  }

  return results;
}

std::vector<DistanceMapImageResult> createDistanceMapImages(const Image& image, float downsamplingFactor)
{
  if (image.header().interleavedComponents()) {
    spdlog::info(
      "Image has multiple, interleaved components, "
      "so the distance map will not be computed");
    return {};
  }

  using TItkImageComp = float;
  using TDistMapComp = uint8_t; // to save GPU memory

  std::vector<DistanceMapImageResult> results;
  results.reserve(image.header().numComponentsPerPixel());

  for (uint32_t comp = 0; comp < image.header().numComponentsPerPixel(); ++comp) {
    const auto compImage = createItkImageFromImageComponent<TItkImageComp>(image, comp);

    const auto thresholds = image.settings().foregroundThresholds(comp);
    const float threshLow = static_cast<float>(thresholds.first);
    const float threshHigh = static_cast<float>(thresholds.second);

    spdlog::debug("Computing Euclidean distance map using thresholds {} and {}", threshLow, threshHigh);

    const auto distMapItkImage = computeEuclideanDistanceMap<TItkImageComp, TDistMapComp>(
      compImage,
      comp,
      threshLow,
      threshHigh,
      std::min(downsamplingFactor, 1.0f));

    if (!distMapItkImage) {
      spdlog::warn("Unable to create distance map for component {}", comp);
      continue;
    }

    const std::string displayName =
      std::string("Dist map for comp ") + std::to_string(comp) + " of '" + image.settings().displayName() + "'";

    results.push_back(DistanceMapImageResult{
      comp,
      createImageFromItkImage<TDistMapComp>(distMapItkImage, displayName),
      thresholds.second});
  }

  return results;
}

entropy_expected::expected<Image, std::string> createComponentProjectionImage(
  const Image& image,
  ComponentProjectionMode mode)
{
  const uint32_t numComponents = image.header().numComponentsPerPixel();
  if (numComponents < 2) {
    return entropy_expected::unexpected("Component projection requires at least two image components");
  }

  if (!image.hasPixelData()) {
    return entropy_expected::unexpected("Component projection requires loaded image pixel data");
  }

  const bool isComplexPhaseProjection = ComponentProjectionMode::ComplexPhaseSignedRadians == mode ||
                                        ComponentProjectionMode::ComplexPhaseUnsignedRadians == mode ||
                                        ComponentProjectionMode::ComplexPhaseSignedDegrees == mode ||
                                        ComponentProjectionMode::ComplexPhaseUnsignedDegrees == mode;
  if (isComplexPhaseProjection && !isComplexValuedImage(image)) {
    return entropy_expected::unexpected("Complex phase projection requires a two-component complex image");
  }

  const std::size_t numPixels = image.header().numPixels();
  std::vector<float> values(numPixels, 0.0f);
  std::size_t nonFiniteValueCount = 0;

  for (std::size_t pixel = 0; pixel < numPixels; ++pixel) {
    double minValue = std::numeric_limits<double>::max();
    double maxValue = std::numeric_limits<double>::lowest();
    double sum = 0.0;
    double sumSquares = 0.0;
    uint32_t finiteComponentCount = 0;

    for (uint32_t component = 0; component < numComponents; ++component) {
      const auto value = image.value<double>(component, pixel);
      if (!value) {
        return entropy_expected::unexpected(std::format("Unable to read component {} at pixel {}", component, pixel));
      }

      if (!std::isfinite(*value)) {
        ++nonFiniteValueCount;
        continue;
      }

      minValue = std::min(minValue, *value);
      maxValue = std::max(maxValue, *value);
      sum += *value;
      sumSquares += (*value) * (*value);
      ++finiteComponentCount;
    }

    if (finiteComponentCount == 0) {
      values[pixel] = 0.0f;
      continue;
    }

    switch (mode) {
      case ComponentProjectionMode::Minimum:
        values[pixel] = static_cast<float>(minValue);
        break;
      case ComponentProjectionMode::Mean:
        values[pixel] = static_cast<float>(sum / static_cast<double>(finiteComponentCount));
        break;
      case ComponentProjectionMode::Maximum:
        values[pixel] = static_cast<float>(maxValue);
        break;
      case ComponentProjectionMode::Magnitude:
        values[pixel] = static_cast<float>(std::sqrt(sumSquares));
        break;
      case ComponentProjectionMode::ComplexPhaseSignedRadians:
      case ComponentProjectionMode::ComplexPhaseUnsignedRadians:
      case ComponentProjectionMode::ComplexPhaseSignedDegrees:
      case ComponentProjectionMode::ComplexPhaseUnsignedDegrees: {
        const auto real = image.value<double>(0, pixel);
        const auto imaginary = image.value<double>(1, pixel);
        if (!real || !imaginary) {
          return entropy_expected::unexpected(std::format("Unable to read complex value at pixel {}", pixel));
        }
        if (!std::isfinite(*real) || !std::isfinite(*imaginary)) {
          values[pixel] = 0.0f;
          break;
        }
        const ComplexPhaseRange range = (ComponentProjectionMode::ComplexPhaseUnsignedRadians == mode ||
                                         ComponentProjectionMode::ComplexPhaseUnsignedDegrees == mode)
                                          ? ComplexPhaseRange::Unsigned
                                          : ComplexPhaseRange::Signed;
        const ComplexPhaseUnit unit = (ComponentProjectionMode::ComplexPhaseSignedDegrees == mode ||
                                       ComponentProjectionMode::ComplexPhaseUnsignedDegrees == mode)
                                        ? ComplexPhaseUnit::Degrees
                                        : ComplexPhaseUnit::Radians;
        values[pixel] = static_cast<float>(complexPhaseValue(*real, *imaginary, range, unit));
        break;
      }
    }
  }

  if (nonFiniteValueCount > 0) {
    spdlog::warn(
      "Ignored {} non-finite component value(s) while creating {} projection for image '{}'",
      nonFiniteValueCount,
      componentProjectionModeName(mode),
      image.settings().displayName());
  }

  ImageHeader header = image.header();
  header.adjustComponents(ComponentType::Float32, 1);
  header.setExistsOnDisk(false);

  const std::string displayName = image.settings().displayName() + " " + componentProjectionModeName(mode);
  Image projection(
    header,
    displayName,
    Image::ImageRepresentation::Image,
    Image::MultiComponentBufferType::SeparateImages,
    std::vector<const void*>{values.data()});
  projection.transformations() = image.transformations();
  projection.settings().setBorderColor(image.settings().borderColor());
  projection.settings().setGlobalVisibility(image.settings().globalVisibility());
  projection.settings().setGlobalOpacity(image.settings().globalOpacity());
  projection.settings().setVisibility(image.settings().visibility());
  projection.settings().setOpacity(image.settings().opacity());
  projection.settings().setInterpolationMode(image.settings().interpolationMode());
  projection.settings().setColorMapIndex(image.settings().colorMapIndex());
  projection.settings().setUseDistanceMapForRaycasting(false);

  return projection;
}
