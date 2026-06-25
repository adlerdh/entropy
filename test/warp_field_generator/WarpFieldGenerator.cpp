#include "WarpFieldGenerator.h"

#include <glm/geometric.hpp>
#include <glm/matrix.hpp>

#include <itkImageFileWriter.h>
#include <itkImageRegionIterator.h>
#include <itkVectorImage.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <limits>
#include <numbers>
#include <sstream>
#include <stdexcept>
#include <string>

namespace warp_field
{
namespace
{
using json = nlohmann::json;

constexpr double kMinPositive = 1.0e-12;

std::string lower(std::string text)
{
  std::ranges::transform(text, text.begin(), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  return text;
}

std::string requireString(const json& value, const char* key)
{
  if (!value.contains(key) || !value.at(key).is_string()) {
    throw std::runtime_error(std::string("Missing string key: ") + key);
  }
  return value.at(key).get<std::string>();
}

template<class T>
std::vector<T> optionalVector(const json& value, const char* key, std::vector<T> fallback)
{
  if (!value.contains(key)) {
    return fallback;
  }
  return value.at(key).get<std::vector<T>>();
}

glm::dvec3 optionalVector3(const json& value, const char* key, glm::dvec3 fallback)
{
  if (!value.contains(key)) {
    return fallback;
  }

  const std::vector<double> raw = value.at(key).get<std::vector<double>>();
  if (raw.size() > kMaxSpatialDimension) {
    throw std::runtime_error(std::string("Expected at most three values for key: ") + key);
  }

  for (std::size_t i = 0; i < raw.size(); ++i) {
    fallback[i] = raw[i];
  }
  return fallback;
}

glm::dmat3 optionalMatrix3(const json& value, const char* key, glm::dmat3 fallback)
{
  if (!value.contains(key)) {
    return fallback;
  }

  const std::vector<double> raw = value.at(key).get<std::vector<double>>();
  if (raw.size() != kMaxSpatialDimension * kMaxSpatialDimension) {
    throw std::runtime_error(std::string("Expected nine row-major matrix values for key: ") + key);
  }

  for (std::size_t row = 0; row < kMaxSpatialDimension; ++row) {
    for (std::size_t col = 0; col < kMaxSpatialDimension; ++col) {
      fallback[col][row] = raw[(row * kMaxSpatialDimension) + col];
    }
  }
  return fallback;
}

glm::dvec3 vector3FromJson(const json& value, const char* key)
{
  if (!value.contains(key)) {
    throw std::runtime_error(std::string("Missing vector key: ") + key);
  }

  const std::vector<double> raw = value.at(key).get<std::vector<double>>();
  if (raw.size() > kMaxSpatialDimension) {
    throw std::runtime_error(std::string("Expected at most three values for key: ") + key);
  }

  glm::dvec3 out{0.0, 0.0, 0.0};
  for (std::size_t i = 0; i < raw.size(); ++i) {
    out[i] = raw[i];
  }
  return out;
}

void parseControlPointArray(const json& value, WarpOperation& operation)
{
  if (!value.contains("control_points")) {
    return;
  }

  for (const json& pointSpec : value.at("control_points")) {
    operation.controlPoints.push_back(vector3FromJson(pointSpec, "point"));
    operation.displacements.push_back(vector3FromJson(pointSpec, "displacement"));
    operation.weights.push_back(pointSpec.value("radius", operation.width));
  }
}

void parseLandmarkArray(const json& value, WarpOperation& operation)
{
  if (!value.contains("landmarks")) {
    return;
  }

  for (const json& landmarkSpec : value.at("landmarks")) {
    const glm::dvec3 fixed = vector3FromJson(landmarkSpec, "fixed");
    const glm::dvec3 moving = vector3FromJson(landmarkSpec, "moving");
    operation.controlPoints.push_back(fixed);
    operation.displacements.push_back(moving - fixed);
    operation.weights.push_back(landmarkSpec.value("radius", operation.width));
  }
}

OperationType parseOperationType(const std::string& text)
{
  const std::string type = lower(text);
  if (type == "affine" || type == "linear") {
    return OperationType::Affine;
  }
  if (type == "inverse_affine" || type == "known_inverse" || type == "inverse") {
    return OperationType::InverseAffine;
  }
  if (type == "expansion" || type == "expand" || type == "elliptical_expansion" || type == "spherical_expansion") {
    return OperationType::Expansion;
  }
  if (type == "contraction" || type == "contract" || type == "elliptical_contraction" ||
      type == "spherical_contraction") {
    return OperationType::Contraction;
  }
  if (type == "rotation" || type == "rotate" || type == "pure_rotation") {
    return OperationType::Rotation;
  }
  if (type == "twist" || type == "twisting") {
    return OperationType::Twist;
  }
  if (type == "swirl" || type == "radial_swirl") {
    return OperationType::Swirl;
  }
  if (type == "vortex_pair" || type == "vortexpair") {
    return OperationType::VortexPair;
  }
  if (type == "source_sink_pair" || type == "sourcesinkpair" || type == "sink_source_pair") {
    return OperationType::SourceSinkPair;
  }
  if (type == "wave" || type == "waves") {
    return OperationType::Wave;
  }
  if (type == "shear") {
    return OperationType::Shear;
  }
  if (type == "stretch" || type == "scale" || type == "scaling") {
    return OperationType::Stretch;
  }
  if (type == "fold" || type == "folding") {
    return OperationType::Fold;
  }
  if (type == "tear" || type == "tearing") {
    return OperationType::Tear;
  }
  if (type == "control_point" || type == "control_points" || type == "bspline" || type == "b_spline") {
    return OperationType::ControlPoint;
  }
  if (type == "smooth_random" || type == "smooth_noise") {
    return OperationType::SmoothRandom;
  }
  if (type == "piecewise_affine") {
    return OperationType::PiecewiseAffine;
  }
  if (type == "sliding_interface" || type == "sliding") {
    return OperationType::SlidingInterface;
  }
  if (type == "divergence_free" || type == "incompressible") {
    return OperationType::DivergenceFree;
  }
  if (type == "curl_free" || type == "potential") {
    return OperationType::CurlFree;
  }
  if (type == "jacobian_expansion" || type == "jacobian_controlled") {
    return OperationType::JacobianExpansion;
  }
  if (type == "landmark" || type == "landmark_driven" || type == "landmarks") {
    return OperationType::Landmark;
  }
  if (type == "boundary_constrained" || type == "edge_zero") {
    return OperationType::BoundaryConstrained;
  }
  if (type == "jump" || type == "discontinuous_jump" || type == "step") {
    return OperationType::Jump;
  }
  if (type == "white_noise" || type == "noise") {
    return OperationType::WhiteNoise;
  }
  throw std::runtime_error("Unknown warp operation type: " + text);
}

TimeMode parseTimeMode(const std::string& text)
{
  const std::string mode = lower(text);
  if (mode == "constant" || mode == "none") {
    return TimeMode::Constant;
  }
  if (mode == "sine" || mode == "sin" || mode == "pulsing") {
    return TimeMode::Sine;
  }
  if (mode == "cosine" || mode == "cos") {
    return TimeMode::Cosine;
  }
  if (mode == "ramp" || mode == "linear") {
    return TimeMode::Ramp;
  }
  throw std::runtime_error("Unknown time mode: " + text);
}

CompositionMode parseCompositionMode(const std::string& text)
{
  const std::string mode = lower(text);
  if (mode == "additive" || mode == "add" || mode == "sum" || mode == "summed") {
    return CompositionMode::Additive;
  }
  if (mode == "composed" || mode == "compose" || mode == "sequential") {
    return CompositionMode::Composed;
  }
  throw std::runtime_error("Unknown composition mode: " + text);
}

TimeProfile parseTimeProfile(const json& value)
{
  TimeProfile profile;
  if (!value.is_object()) {
    return profile;
  }

  profile.mode = parseTimeMode(value.value("mode", "constant"));
  profile.amplitude = value.value("amplitude", profile.amplitude);
  profile.frequency = value.value("frequency", profile.frequency);
  profile.phase = value.value("phase", profile.phase);
  profile.offset = value.value("offset", profile.offset);
  return profile;
}

WarpOperation parseOperation(const json& value)
{
  WarpOperation operation;
  operation.type = parseOperationType(requireString(value, "type"));
  operation.time = parseTimeProfile(value.value("time", json::object()));
  operation.center = optionalVector3(value, "center", operation.center);
  operation.secondaryCenter = optionalVector3(value, "secondary_center", operation.secondaryCenter);
  operation.radii = optionalVector3(value, "radii", operation.radii);
  operation.direction = optionalVector3(value, "direction", operation.direction);
  operation.secondaryDirection = optionalVector3(value, "secondary_direction", operation.secondaryDirection);
  operation.axis = optionalVector3(value, "axis", operation.axis);
  operation.translation = optionalVector3(value, "translation", operation.translation);
  operation.secondaryTranslation = optionalVector3(value, "secondary_translation", operation.secondaryTranslation);
  operation.scale = optionalVector3(value, "scale", operation.scale);
  operation.normal = optionalVector3(value, "normal", operation.normal);
  operation.matrix = optionalMatrix3(value, "matrix", operation.matrix);
  operation.secondaryMatrix = optionalMatrix3(value, "secondary_matrix", operation.secondaryMatrix);
  operation.amplitude = value.value("amplitude", operation.amplitude);
  operation.frequency = value.value("frequency", operation.frequency);
  operation.phase = value.value("phase", operation.phase);
  operation.width = value.value("width", operation.width);
  operation.cellSize = value.value("cell_size", operation.cellSize);
  operation.seed = value.value("seed", operation.seed);
  parseControlPointArray(value, operation);
  parseLandmarkArray(value, operation);
  return operation;
}

double timeScale(const TimeProfile& profile, double normalizedTime)
{
  const double angle = (2.0 * std::numbers::pi * profile.frequency * normalizedTime) + profile.phase;
  switch (profile.mode) {
    case TimeMode::Constant:
      return profile.amplitude + profile.offset;
    case TimeMode::Sine:
      return (profile.amplitude * std::sin(angle)) + profile.offset;
    case TimeMode::Cosine:
      return (profile.amplitude * std::cos(angle)) + profile.offset;
    case TimeMode::Ramp:
      return (profile.amplitude * normalizedTime) + profile.offset;
  }
  return 1.0;
}

double dotN(const glm::dvec3& lhs, const glm::dvec3& rhs, std::size_t dimension)
{
  double value = 0.0;
  for (std::size_t i = 0; i < dimension; ++i) {
    value += lhs[i] * rhs[i];
  }
  return value;
}

double norm(const glm::dvec3& value, std::size_t dimension)
{
  return std::sqrt(dotN(value, value, dimension));
}

glm::dvec3 rotateAroundAxis(const glm::dvec3& value, const glm::dvec3& axis, double angle)
{
  const glm::dvec3 unitAxis = normalized(axis);
  const double c = std::cos(angle);
  const double s = std::sin(angle);
  const double axisProjection = glm::dot(unitAxis, value);

  return (value * c) + (glm::cross(unitAxis, value) * s) + (unitAxis * axisProjection * (1.0 - c));
}

glm::dvec3 affineDisplacement(const glm::dmat3& matrix, const glm::dvec3& translation, const glm::dvec3& relative, std::size_t dimension)
{
  glm::dvec3 out = translation;
  for (std::size_t row = 0; row < dimension; ++row) {
    for (std::size_t col = 0; col < dimension; ++col) {
      const double identity = row == col ? 1.0 : 0.0;
      out[row] += (matrix[col][row] - identity) * relative[col];
    }
  }
  return out;
}

double ellipsoidWeight(const WarpOperation& operation, const glm::dvec3& relative, std::size_t dimension)
{
  double radiusSquared = 0.0;
  for (std::size_t i = 0; i < dimension; ++i) {
    const double radius = std::max(std::abs(operation.radii[i]), kMinPositive);
    const double normalizedCoordinate = relative[i] / radius;
    radiusSquared += normalizedCoordinate * normalizedCoordinate;
  }
  return std::exp(-0.5 * radiusSquared);
}

double boundaryEnvelope(const WarpOperation& operation, const glm::dvec3& relative, std::size_t dimension)
{
  double envelope = 1.0;
  for (std::size_t i = 0; i < dimension; ++i) {
    const double radius = std::max(std::abs(operation.radii[i]), kMinPositive);
    const double normalizedCoordinate = std::clamp(std::abs(relative[i]) / radius, 0.0, 1.0);
    envelope *= std::sin(std::numbers::pi * normalizedCoordinate);
  }
  return envelope;
}

glm::dvec3 radialDisplacement(
  const WarpOperation& operation,
  const glm::dvec3& point,
  const glm::dvec3& center,
  double sign,
  std::size_t dimension)
{
  const glm::dvec3 relative = point - center;
  glm::dvec3 radial = relative;
  const double radialNorm = norm(radial, dimension);
  if (radialNorm <= kMinPositive) {
    radial = normalized(operation.direction);
  }
  else {
    radial /= radialNorm;
  }

  return radial * sign * operation.amplitude * ellipsoidWeight(operation, relative, dimension);
}

glm::dvec3 tangentialDisplacement(
  const WarpOperation& operation,
  const glm::dvec3& point,
  const glm::dvec3& center,
  double sign,
  std::size_t dimension)
{
  const glm::dvec3 relative = point - center;
  glm::dvec3 tangent = glm::cross(normalized(operation.axis), relative);
  const double tangentNorm = norm(tangent, dimension);
  if (tangentNorm <= kMinPositive) {
    return {0.0, 0.0, 0.0};
  }

  tangent /= tangentNorm;
  return tangent * sign * operation.amplitude * ellipsoidWeight(operation, relative, dimension);
}

std::uint32_t hashCoordinate(std::int64_t x, std::int64_t y, std::int64_t z, unsigned int seed)
{
  std::uint64_t value = static_cast<std::uint64_t>(seed) + 0x9e3779b97f4a7c15ULL;
  value ^= static_cast<std::uint64_t>(x) + 0xbf58476d1ce4e5b9ULL + (value << 6U) + (value >> 2U);
  value ^= static_cast<std::uint64_t>(y) + 0x94d049bb133111ebULL + (value << 6U) + (value >> 2U);
  value ^= static_cast<std::uint64_t>(z) + 0x2545f4914f6cdd1dULL + (value << 6U) + (value >> 2U);
  value ^= value >> 30U;
  value *= 0xbf58476d1ce4e5b9ULL;
  value ^= value >> 27U;
  value *= 0x94d049bb133111ebULL;
  value ^= value >> 31U;
  return static_cast<std::uint32_t>(value & 0xffffffffU);
}

double unitNoise(std::int64_t x, std::int64_t y, std::int64_t z, unsigned int seed, std::size_t component)
{
  const std::uint32_t raw = hashCoordinate(x, y, z, seed + static_cast<unsigned int>(101U * component));
  return (static_cast<double>(raw) / static_cast<double>(std::numeric_limits<std::uint32_t>::max())) * 2.0 - 1.0;
}

double smoothStep(double value)
{
  const double t = std::clamp(value, 0.0, 1.0);
  return t * t * (3.0 - (2.0 * t));
}

double lerp(double a, double b, double t)
{
  return a + ((b - a) * t);
}

glm::dvec3 proceduralNoise(const WarpOperation& operation, const glm::dvec3& point, bool smooth)
{
  const double cellSize = std::max(std::abs(operation.cellSize), kMinPositive);
  const double x = point[0] / cellSize;
  const double y = point[1] / cellSize;
  const double z = point[2] / cellSize;
  const auto ix = static_cast<std::int64_t>(std::floor(x));
  const auto iy = static_cast<std::int64_t>(std::floor(y));
  const auto iz = static_cast<std::int64_t>(std::floor(z));

  if (!smooth) {
    return {
      operation.amplitude * unitNoise(ix, iy, iz, operation.seed, 0),
      operation.amplitude * unitNoise(ix, iy, iz, operation.seed, 1),
      operation.amplitude * unitNoise(ix, iy, iz, operation.seed, 2)};
  }

  const double tx = smoothStep(x - static_cast<double>(ix));
  const double ty = smoothStep(y - static_cast<double>(iy));
  const double tz = smoothStep(z - static_cast<double>(iz));

  glm::dvec3 out{0.0, 0.0, 0.0};
  for (std::size_t component = 0; component < kMaxSpatialDimension; ++component) {
    const double c000 = unitNoise(ix, iy, iz, operation.seed, component);
    const double c100 = unitNoise(ix + 1, iy, iz, operation.seed, component);
    const double c010 = unitNoise(ix, iy + 1, iz, operation.seed, component);
    const double c110 = unitNoise(ix + 1, iy + 1, iz, operation.seed, component);
    const double c001 = unitNoise(ix, iy, iz + 1, operation.seed, component);
    const double c101 = unitNoise(ix + 1, iy, iz + 1, operation.seed, component);
    const double c011 = unitNoise(ix, iy + 1, iz + 1, operation.seed, component);
    const double c111 = unitNoise(ix + 1, iy + 1, iz + 1, operation.seed, component);
    const double c00 = lerp(c000, c100, tx);
    const double c10 = lerp(c010, c110, tx);
    const double c01 = lerp(c001, c101, tx);
    const double c11 = lerp(c011, c111, tx);
    const double c0 = lerp(c00, c10, ty);
    const double c1 = lerp(c01, c11, ty);
    out[component] = operation.amplitude * lerp(c0, c1, tz);
  }
  return out;
}

glm::dvec3 controlPointDisplacement(const WarpOperation& operation, const glm::dvec3& point, std::size_t dimension)
{
  glm::dvec3 weightedSum{0.0, 0.0, 0.0};
  double totalWeight = 0.0;

  for (std::size_t i = 0; i < operation.controlPoints.size(); ++i) {
    const glm::dvec3 relative = point - operation.controlPoints[i];
    const double radius = std::max(
      i < operation.weights.size() ? std::abs(operation.weights[i]) : std::abs(operation.width),
      kMinPositive);
    const double distance = norm(relative, dimension) / radius;
    const double weight = std::exp(-0.5 * distance * distance);
    weightedSum += operation.displacements[i] * weight;
    totalWeight += weight;
  }

  if (totalWeight <= kMinPositive) {
    return {0.0, 0.0, 0.0};
  }
  return weightedSum / totalWeight;
}

glm::dvec3 evaluateOperation(
  const WarpOperation& operation,
  const glm::dvec3& point,
  std::size_t spatialDimension,
  double normalizedTime)
{
  const double temporalScale = timeScale(operation.time, normalizedTime);
  const glm::dvec3 relative = point - operation.center;

  switch (operation.type) {
    case OperationType::Affine: {
      return affineDisplacement(operation.matrix, operation.translation, relative, spatialDimension) * temporalScale;
    }
    case OperationType::InverseAffine: {
      const glm::dmat3 inverse = glm::inverse(operation.matrix);
      const glm::dvec3 inverseRelative = inverse * (relative - operation.translation);
      return (inverseRelative - relative) * temporalScale;
    }
    case OperationType::Expansion:
    case OperationType::Contraction: {
      const double sign = operation.type == OperationType::Expansion ? 1.0 : -1.0;
      return radialDisplacement(operation, point, operation.center, sign, spatialDimension) * temporalScale;
    }
    case OperationType::Rotation: {
      const double angle = operation.amplitude * temporalScale;
      return rotateAroundAxis(relative, operation.axis, angle) - relative;
    }
    case OperationType::Twist: {
      const double radiusWeight = ellipsoidWeight(operation, relative, spatialDimension);
      const double angle = operation.amplitude * radiusWeight * temporalScale;
      return rotateAroundAxis(relative, operation.axis, angle) - relative;
    }
    case OperationType::Swirl:
      return tangentialDisplacement(operation, point, operation.center, 1.0, spatialDimension) * temporalScale;
    case OperationType::VortexPair:
      return (tangentialDisplacement(operation, point, operation.center, 1.0, spatialDimension) +
              tangentialDisplacement(operation, point, operation.secondaryCenter, -1.0, spatialDimension)) *
             temporalScale;
    case OperationType::SourceSinkPair:
      return (radialDisplacement(operation, point, operation.center, 1.0, spatialDimension) +
              radialDisplacement(operation, point, operation.secondaryCenter, -1.0, spatialDimension)) *
             temporalScale;
    case OperationType::Wave: {
      const glm::dvec3 unitNormal = normalized(operation.normal);
      const glm::dvec3 unitDirection = normalized(operation.direction);
      const double phase =
        (2.0 * std::numbers::pi * operation.frequency * dotN(point, unitNormal, spatialDimension)) + operation.phase;
      return unitDirection * operation.amplitude * std::sin(phase) * temporalScale;
    }
    case OperationType::Shear: {
      const glm::dvec3 unitNormal = normalized(operation.normal);
      const glm::dvec3 unitDirection = normalized(operation.direction);
      const double amount = operation.amplitude * dotN(relative, unitNormal, spatialDimension);
      return unitDirection * amount * temporalScale;
    }
    case OperationType::Stretch: {
      glm::dvec3 out{0.0, 0.0, 0.0};
      for (std::size_t i = 0; i < spatialDimension; ++i) {
        out[i] = (operation.scale[i] - 1.0) * relative[i];
      }
      return out * operation.amplitude * temporalScale;
    }
    case OperationType::PiecewiseAffine: {
      const glm::dvec3 unitNormal = normalized(operation.normal);
      const double signedDistance = dotN(relative, unitNormal, spatialDimension);
      const bool primarySide = signedDistance < 0.0;
      const glm::dmat3& matrix = primarySide ? operation.matrix : operation.secondaryMatrix;
      const glm::dvec3& translation = primarySide ? operation.translation : operation.secondaryTranslation;
      return affineDisplacement(matrix, translation, relative, spatialDimension) * temporalScale;
    }
    case OperationType::Fold:
    case OperationType::Tear: {
      const glm::dvec3 unitNormal = normalized(operation.normal);
      const glm::dvec3 unitDirection = normalized(operation.direction);
      const double signedDistance = dotN(relative, unitNormal, spatialDimension);
      const double width = std::max(std::abs(operation.width), kMinPositive);
      const double profile = std::tanh(signedDistance / width);
      const double foldScale = operation.type == OperationType::Fold ? profile : 0.5 * (1.0 + profile);
      return unitDirection * operation.amplitude * foldScale * temporalScale;
    }
    case OperationType::SlidingInterface: {
      const glm::dvec3 unitNormal = normalized(operation.normal);
      const glm::dvec3 unitDirection = normalized(operation.direction);
      const double signedDistance = dotN(relative, unitNormal, spatialDimension);
      const double width = std::max(std::abs(operation.width), kMinPositive);
      const double profile = std::tanh(signedDistance / width);
      return unitDirection * operation.amplitude * profile * temporalScale;
    }
    case OperationType::ControlPoint:
    case OperationType::Landmark:
      return controlPointDisplacement(operation, point, spatialDimension) * operation.amplitude * temporalScale;
    case OperationType::SmoothRandom:
      return proceduralNoise(operation, point, true) * temporalScale;
    case OperationType::WhiteNoise:
      return proceduralNoise(operation, point, false) * temporalScale;
    case OperationType::DivergenceFree: {
      const double k = 2.0 * std::numbers::pi * operation.frequency;
      const double x = point[0] - operation.center[0];
      const double y = point[1] - operation.center[1];
      return {
        operation.amplitude * k * std::cos(k * y + operation.phase) * temporalScale,
        -operation.amplitude * k * std::cos(k * x + operation.phase) * temporalScale,
        0.0};
    }
    case OperationType::CurlFree: {
      const glm::dvec3 unitNormal = normalized(operation.normal);
      const double k = 2.0 * std::numbers::pi * operation.frequency;
      const double phase = k * dotN(relative, unitNormal, spatialDimension) + operation.phase;
      return unitNormal * operation.amplitude * k * std::cos(phase) * temporalScale;
    }
    case OperationType::JacobianExpansion: {
      const double scale = std::cbrt(std::max(operation.amplitude, kMinPositive));
      glm::dvec3 out{0.0, 0.0, 0.0};
      for (std::size_t i = 0; i < spatialDimension; ++i) {
        out[i] = (scale - 1.0) * relative[i];
      }
      return out * temporalScale;
    }
    case OperationType::BoundaryConstrained: {
      const glm::dvec3 unitDirection = normalized(operation.direction);
      const double envelope = boundaryEnvelope(operation, relative, spatialDimension);
      return unitDirection * operation.amplitude * envelope * temporalScale;
    }
    case OperationType::Jump: {
      const glm::dvec3 unitNormal = normalized(operation.normal);
      const glm::dvec3 unitDirection = normalized(operation.direction);
      const double signedDistance = dotN(relative, unitNormal, spatialDimension);
      return signedDistance >= 0.0 ? unitDirection * operation.amplitude * temporalScale : glm::dvec3{0.0, 0.0, 0.0};
    }
  }

  return {0.0, 0.0, 0.0};
}

std::size_t imageDimension(const WarpFieldSpec& spec)
{
  return spec.size.size();
}

std::vector<double> identityDirection(std::size_t dimension)
{
  std::vector<double> direction(dimension * dimension, 0.0);
  for (std::size_t i = 0; i < dimension; ++i) {
    direction[(i * dimension) + i] = 1.0;
  }
  return direction;
}

std::vector<double> expandedDirection(
  const std::vector<double>& direction,
  std::size_t spatialDimension,
  std::size_t dimension)
{
  if (direction.size() == dimension * dimension) {
    return direction;
  }

  if (dimension == spatialDimension + 1 && direction.size() == spatialDimension * spatialDimension) {
    std::vector<double> expanded = identityDirection(dimension);
    for (std::size_t row = 0; row < spatialDimension; ++row) {
      for (std::size_t col = 0; col < spatialDimension; ++col) {
        expanded[(row * dimension) + col] = direction[(row * spatialDimension) + col];
      }
    }
    return expanded;
  }

  return direction;
}

template<class ComponentType, unsigned int ImageDimension>
void writeTypedWarpField(const WarpFieldSpec& spec)
{
  using ImageType = itk::VectorImage<ComponentType, ImageDimension>;
  using WriterType = itk::ImageFileWriter<ImageType>;
  using IteratorType = itk::ImageRegionIterator<ImageType>;
  using PixelType = typename ImageType::PixelType;

  auto image = ImageType::New();
  typename ImageType::RegionType region;
  typename ImageType::SizeType size;
  typename ImageType::IndexType start;
  typename ImageType::SpacingType spacing;
  typename ImageType::PointType origin;
  typename ImageType::DirectionType direction;

  direction.SetIdentity();
  for (unsigned int row = 0; row < ImageDimension; ++row) {
    for (unsigned int col = 0; col < ImageDimension; ++col) {
      direction[row][col] = spec.direction[(row * ImageDimension) + col];
    }
  }

  for (unsigned int i = 0; i < ImageDimension; ++i) {
    start[i] = 0;
    size[i] = spec.size[i];
    spacing[i] = spec.spacing[i];
    origin[i] = spec.origin[i];
  }

  region.SetIndex(start);
  region.SetSize(size);
  image->SetRegions(region);
  image->SetVectorLength(spec.vectorDimension);
  image->SetSpacing(spacing);
  image->SetOrigin(origin);
  image->SetDirection(direction);
  image->Allocate();

  IteratorType iterator(image, image->GetLargestPossibleRegion());
  for (iterator.GoToBegin(); !iterator.IsAtEnd(); ++iterator) {
    const typename ImageType::IndexType index = iterator.GetIndex();
    typename ImageType::PointType physicalPoint;
    image->TransformIndexToPhysicalPoint(index, physicalPoint);

    glm::dvec3 spatialPoint{0.0, 0.0, 0.0};
    for (std::size_t i = 0; i < spec.spatialDimension; ++i) {
      spatialPoint[i] = physicalPoint[static_cast<unsigned int>(i)];
    }

    double normalizedTime = 0.0;
    if constexpr (ImageDimension > 1) {
      if (ImageDimension > spec.spatialDimension && spec.size[spec.spatialDimension] > 1) {
        normalizedTime = static_cast<double>(index[spec.spatialDimension]) /
                         static_cast<double>(spec.size[spec.spatialDimension] - 1);
      }
    }

    const glm::dvec3 displacement = evaluateDisplacement(spec, spatialPoint, normalizedTime);
    PixelType pixel;
    pixel.SetSize(spec.vectorDimension);
    for (std::size_t component = 0; component < spec.vectorDimension; ++component) {
      pixel[component] = static_cast<ComponentType>(displacement[component]);
    }
    iterator.Set(pixel);
  }

  auto writer = WriterType::New();
  writer->SetFileName(spec.output.string());
  writer->SetInput(image);
  writer->SetUseCompression(true);
  writer->Update();
}

template<class ComponentType>
void writeTypedWarpField(const WarpFieldSpec& spec)
{
  switch (imageDimension(spec)) {
    case 1:
      writeTypedWarpField<ComponentType, 1>(spec);
      return;
    case 2:
      writeTypedWarpField<ComponentType, 2>(spec);
      return;
    case 3:
      writeTypedWarpField<ComponentType, 3>(spec);
      return;
    case 4:
      writeTypedWarpField<ComponentType, 4>(spec);
      return;
    default:
      throw std::runtime_error("Unsupported image dimension");
  }
}

} // namespace

glm::dvec3 normalized(const glm::dvec3& vector)
{
  const double length = norm(vector, kMaxSpatialDimension);
  if (length <= kMinPositive) {
    return {0.0, 0.0, 0.0};
  }
  return vector / length;
}

WarpFieldSpec parseSpecJson(const std::string& text)
{
  const json document = json::parse(text);
  WarpFieldSpec spec;

  if (document.contains("output")) {
    spec.output = document.at("output").get<std::string>();
  }
  spec.componentType = document.value("component_type", spec.componentType);
  spec.spatialDimension = document.value("spatial_dimension", spec.spatialDimension);
  spec.vectorDimension = document.value("vector_dimension", spec.spatialDimension);
  spec.size = optionalVector<std::size_t>(document, "size", std::vector<std::size_t>(spec.spatialDimension, 64));
  spec.spacing = optionalVector<double>(document, "spacing", std::vector<double>(spec.size.size(), 1.0));
  spec.origin = optionalVector<double>(document, "origin", std::vector<double>(spec.size.size(), 0.0));
  spec.compositionMode = parseCompositionMode(document.value("composition_mode", document.value("composition", "additive")));

  if (document.contains("time_points")) {
    const std::size_t timePoints = document.at("time_points").get<std::size_t>();
    if (spec.size.size() == spec.spatialDimension) {
      spec.size.push_back(timePoints);
      spec.spacing.push_back(document.value("time_spacing", 1.0));
      spec.origin.push_back(document.value("time_origin", 0.0));
    }
  }

  spec.direction = expandedDirection(
    optionalVector<double>(document, "direction", identityDirection(spec.size.size())),
    spec.spatialDimension,
    spec.size.size());

  if (document.contains("operations")) {
    for (const json& operation : document.at("operations")) {
      spec.operations.push_back(parseOperation(operation));
    }
  }

  validateSpec(spec);
  return spec;
}

WarpFieldSpec loadSpecJson(const std::filesystem::path& fileName)
{
  std::ifstream stream(fileName);
  if (!stream) {
    throw std::runtime_error("Cannot open warp field specification: " + fileName.string());
  }

  std::ostringstream buffer;
  buffer << stream.rdbuf();
  WarpFieldSpec spec = parseSpecJson(buffer.str());
  return spec;
}

void validateSpec(const WarpFieldSpec& spec)
{
  if (spec.spatialDimension < 1 || spec.spatialDimension > kMaxSpatialDimension) {
    throw std::runtime_error("spatial_dimension must be 1, 2, or 3");
  }

  if (spec.vectorDimension < 1 || spec.vectorDimension > kMaxSpatialDimension) {
    throw std::runtime_error("vector_dimension must be 1, 2, or 3");
  }

  const std::size_t dimension = imageDimension(spec);
  if (dimension < spec.spatialDimension || dimension > kMaxImageDimension) {
    throw std::runtime_error("size must describe the spatial dimensions and optional time dimension");
  }

  if (dimension > spec.spatialDimension + 1) {
    throw std::runtime_error("only one optional time dimension is supported");
  }

  if (spec.spacing.size() != dimension || spec.origin.size() != dimension) {
    throw std::runtime_error("spacing and origin must match the image dimension");
  }

  if (spec.direction.size() != dimension * dimension) {
    throw std::runtime_error("direction must contain image_dimension * image_dimension values");
  }

  if (spec.operations.empty()) {
    throw std::runtime_error("at least one warp operation is required");
  }

  if (spec.output.empty()) {
    throw std::runtime_error("output image file is required");
  }

  for (const std::size_t extent : spec.size) {
    if (extent == 0) {
      throw std::runtime_error("image dimensions must be greater than zero");
    }
  }

  for (const double spacing : spec.spacing) {
    if (!(spacing > 0.0)) {
      throw std::runtime_error("spacing values must be positive");
    }
  }

  for (const WarpOperation& operation : spec.operations) {
    if ((operation.type == OperationType::ControlPoint || operation.type == OperationType::Landmark) &&
        operation.controlPoints.empty()) {
      throw std::runtime_error("control point and landmark operations require at least one point");
    }

    if (operation.controlPoints.size() != operation.displacements.size()) {
      throw std::runtime_error("control point and displacement counts must match");
    }
  }
}

glm::dvec3 evaluateDisplacement(const WarpFieldSpec& spec, const glm::dvec3& point, double normalizedTime)
{
  if (spec.compositionMode == CompositionMode::Additive) {
    glm::dvec3 displacement{0.0, 0.0, 0.0};
    for (const WarpOperation& operation : spec.operations) {
      displacement += evaluateOperation(operation, point, spec.spatialDimension, normalizedTime);
    }
    return displacement;
  }

  glm::dvec3 currentPoint = point;
  for (const WarpOperation& operation : spec.operations) {
    currentPoint += evaluateOperation(operation, currentPoint, spec.spatialDimension, normalizedTime);
  }
  return currentPoint - point;
}

void writeWarpField(const WarpFieldSpec& spec)
{
  validateSpec(spec);

  const std::string componentType = lower(spec.componentType);
  if (componentType == "float" || componentType == "float32") {
    writeTypedWarpField<float>(spec);
    return;
  }
  if (componentType == "double" || componentType == "float64") {
    writeTypedWarpField<double>(spec);
    return;
  }
  if (componentType == "int16") {
    writeTypedWarpField<std::int16_t>(spec);
    return;
  }
  if (componentType == "uint16") {
    writeTypedWarpField<std::uint16_t>(spec);
    return;
  }
  if (componentType == "int32") {
    writeTypedWarpField<std::int32_t>(spec);
    return;
  }
  if (componentType == "uint32") {
    writeTypedWarpField<std::uint32_t>(spec);
    return;
  }

  throw std::runtime_error("Unsupported component type: " + spec.componentType);
}

} // namespace warp_field
