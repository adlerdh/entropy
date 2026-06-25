#include "image/ImageDerivedData.h"
#include "internal/ImageUtility.tpp"

#include <spdlog/spdlog.h>

#include <glm/geometric.hpp>
#include <glm/mat3x3.hpp>
#include <glm/matrix.hpp>
#include <glm/vec3.hpp>

#include <algorithm>
#include <cmath>
#include <format>
#include <limits>
#include <numbers>
#include <optional>
#include <string>
#include <utility>

namespace
{
bool isVectorDerivativeProjection(ComponentProjectionMode mode)
{
  return ComponentProjectionMode::VectorJacobianDeterminant == mode ||
         ComponentProjectionMode::VectorDivergence == mode || ComponentProjectionMode::VectorCurlMagnitude == mode;
}

std::size_t linearIndex(const glm::uvec3& dims, uint32_t x, uint32_t y, uint32_t z)
{
  return static_cast<std::size_t>(dims.x) * dims.y * z + static_cast<std::size_t>(dims.x) * y + x;
}

double vectorValueAt(const Image& image, uint32_t component, uint32_t x, uint32_t y, uint32_t z)
{
  return image.value<double>(component, static_cast<int>(x), static_cast<int>(y), static_cast<int>(z)).value_or(0.0);
}

double derivativeAt(const Image& image, uint32_t vectorComponent, uint32_t axis, uint32_t x, uint32_t y, uint32_t z)
{
  const glm::uvec3 dims = image.header().pixelDimensions();
  const glm::vec3 spacing = image.header().spacing();

  auto sample = [&](uint32_t coordinate) {
    uint32_t xx = x;
    uint32_t yy = y;
    uint32_t zz = z;
    if (0u == axis) {
      xx = coordinate;
    }
    else if (1u == axis) {
      yy = coordinate;
    }
    else {
      zz = coordinate;
    }
    return vectorValueAt(image, vectorComponent, xx, yy, zz);
  };

  const uint32_t n = dims[axis];
  if (n <= 1u) {
    return 0.0;
  }

  const uint32_t coordinate = 0u == axis ? x : (1u == axis ? y : z);
  const double dx = std::max(static_cast<double>(spacing[axis]), std::numeric_limits<double>::epsilon());

  if (0u == coordinate) {
    return (sample(1u) - sample(0u)) / dx;
  }
  if (coordinate + 1u >= n) {
    return (sample(n - 1u) - sample(n - 2u)) / dx;
  }

  return (sample(coordinate + 1u) - sample(coordinate - 1u)) / (2.0 * dx);
}

entropy_expected::expected<std::vector<float>, std::string> createVectorDerivativeValues(
  const Image& image,
  ComponentProjectionMode mode)
{
  if (!isVectorFieldCandidate(image)) {
    return entropy_expected::unexpected("Vector derivative projection requires a three-component image");
  }

  const glm::uvec3 dims = image.header().pixelDimensions();
  std::vector<float> values(image.header().numPixels(), 0.0f);

  for (uint32_t z = 0; z < dims.z; ++z) {
    for (uint32_t y = 0; y < dims.y; ++y) {
      for (uint32_t x = 0; x < dims.x; ++x) {
        const std::optional<double> value = vectorDerivativeProjectionValue(image, mode, glm::uvec3{x, y, z});
        if (!value) {
          return entropy_expected::unexpected("Unsupported vector derivative projection");
        }

        values[linearIndex(dims, x, y, z)] = static_cast<float>(std::isfinite(*value) ? *value : 0.0);
      }
    }
  }

  return values;
}
} // namespace

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
    case ComponentRenderMode::VectorJacobianDeterminant:
      return ComponentProjectionMode::VectorJacobianDeterminant;
    case ComponentRenderMode::VectorDivergence:
      return ComponentProjectionMode::VectorDivergence;
    case ComponentRenderMode::VectorCurlMagnitude:
      return ComponentProjectionMode::VectorCurlMagnitude;
    case ComponentRenderMode::VectorDirectionColor:
    case ComponentRenderMode::VectorSignedNormalProjection:
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
    case ComponentProjectionMode::VectorJacobianDeterminant:
      return "Jacobian determinant";
    case ComponentProjectionMode::VectorDivergence:
      return "Divergence";
    case ComponentProjectionMode::VectorCurlMagnitude:
      return "Curl magnitude";
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
    case ComponentProjectionMode::VectorJacobianDeterminant:
    case ComponentProjectionMode::VectorDivergence:
    case ComponentProjectionMode::VectorCurlMagnitude:
      return true;
  }

  return false;
}

std::optional<double>
vectorDerivativeProjectionValue(const Image& image, ComponentProjectionMode mode, const glm::uvec3& voxel)
{
  if (!isVectorFieldCandidate(image) || !isVectorDerivativeProjection(mode)) {
    return std::nullopt;
  }

  const glm::uvec3 dims = image.header().pixelDimensions();
  if (voxel.x >= dims.x || voxel.y >= dims.y || voxel.z >= dims.z) {
    return std::nullopt;
  }

  glm::dmat3 jacobian(0.0);
  for (uint32_t component = 0; component < 3u; ++component) {
    for (uint32_t axis = 0; axis < 3u; ++axis) {
      jacobian[axis][component] = derivativeAt(image, component, axis, voxel.x, voxel.y, voxel.z);
    }
  }

  jacobian = jacobian * glm::transpose(glm::dmat3{image.header().directions()});

  if (ComponentProjectionMode::VectorJacobianDeterminant == mode) {
    return glm::determinant(glm::dmat3(1.0) + jacobian);
  }
  if (ComponentProjectionMode::VectorDivergence == mode) {
    return jacobian[0][0] + jacobian[1][1] + jacobian[2][2];
  }
  if (ComponentProjectionMode::VectorCurlMagnitude == mode) {
    const glm::dvec3 curl{
      jacobian[1][2] - jacobian[2][1],
      jacobian[2][0] - jacobian[0][2],
      jacobian[0][1] - jacobian[1][0]};
    return glm::length(curl);
  }

  return std::nullopt;
}

bool isComplexValuedImage(const Image& image)
{
  return PixelType::Complex == image.header().pixelType() && 2u == image.header().numComponentsPerPixel();
}

bool isVectorFieldCandidate(const Image& image)
{
  return 3u == image.header().numComponentsPerPixel();
}

bool isVectorFieldImage(const Image& image)
{
  return isVectorFieldCandidate(image);
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

std::string vectorFieldComponentLabel(uint32_t component, uint32_t componentMax)
{
  std::string label = std::to_string(component) + " of " + std::to_string(componentMax);
  if (0u == component) {
    label = "x (" + label + ")";
  }
  else if (1u == component) {
    label = "y (" + label + ")";
  }
  else if (2u == component) {
    label = "z (" + label + ")";
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
  if (isVectorDerivativeProjection(mode)) {
    auto derivativeValues = createVectorDerivativeValues(image, mode);
    if (!derivativeValues) {
      return entropy_expected::unexpected(derivativeValues.error());
    }
    values = std::move(*derivativeValues);
  }

  std::size_t nonFiniteValueCount = 0;

  for (std::size_t pixel = 0; !isVectorDerivativeProjection(mode) && pixel < numPixels; ++pixel) {
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
      case ComponentProjectionMode::VectorJacobianDeterminant:
      case ComponentProjectionMode::VectorDivergence:
      case ComponentProjectionMode::VectorCurlMagnitude:
        break;
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
