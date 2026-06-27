#pragma once

#include <filesystem>
#include <complex>
#include <map>
#include <string>
#include <vector>

namespace image_generator
{

/**
 * @brief Synthetic image pixel representation to write.
 */
enum class PixelKind
{
  Scalar,
  Vector,
  Complex
};

/**
 * @brief Deterministic intensity pattern used to fill generated images.
 */
enum class Pattern
{
  Ramp,
  Checker,
  Gaussian,
  ComponentRamp,
  ComplexPhase,
  TimeRamp,
  TemporalSine,
  MovingGaussian,
  PulsingGaussian,
  TwoMovingGaussians,
  RotatingWave,
  TimeVaryingWarpField
};

/**
 * @brief Complete synthetic image description.
 */
struct ImageSpec
{
  std::filesystem::path output;
  PixelKind pixelKind = PixelKind::Scalar;
  std::string componentType = "float";
  std::size_t components = 1;
  std::vector<std::size_t> size{32, 32, 16};
  std::vector<double> spacing{1.0, 1.0, 1.0};
  std::vector<double> origin{0.0, 0.0, 0.0};
  std::vector<double> direction{};
  Pattern pattern = Pattern::Ramp;
  double amplitude = 100.0;
  double offset = 0.0;
  std::map<std::string, std::string> metadata;
};

/**
 * @brief Parse an image generation specification from JSON text.
 * @throws std::runtime_error when the JSON is invalid.
 */
ImageSpec parseSpecJson(const std::string& text);

/**
 * @brief Load an image generation specification from disk.
 * @throws std::runtime_error when the file cannot be read or parsed.
 */
ImageSpec loadSpecJson(const std::filesystem::path& fileName);

/**
 * @brief Validate an image generation specification.
 * @throws std::runtime_error when the specification is inconsistent.
 */
void validateSpec(const ImageSpec& spec);

/**
 * @brief Compute the deterministic scalar/vector component value for a generated image.
 * @param spec Image specification.
 * @param index Pixel index, including the time index when the spec has a time dimension.
 * @param component Component index for vector images; ignored for scalar images.
 * @return Value written by writeImage for scalar and vector images before file-format casts.
 */
double expectedComponentValue(const ImageSpec& spec, const std::vector<std::size_t>& index, std::size_t component);

/**
 * @brief Compute the deterministic complex value for a generated complex image.
 * @param spec Image specification.
 * @param index Pixel index, including the time index when the spec has a time dimension.
 * @return Complex value written by writeImage before file-format casts.
 */
std::complex<double> expectedComplexValue(const ImageSpec& spec, const std::vector<std::size_t>& index);

/**
 * @brief Write the requested image to disk.
 * @throws std::runtime_error when validation or writing fails.
 */
void writeImage(const ImageSpec& spec);

} // namespace image_generator
