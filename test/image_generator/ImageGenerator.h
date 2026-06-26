#pragma once

#include <filesystem>
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
  ComplexPhase
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
 * @brief Write the requested image to disk.
 * @throws std::runtime_error when validation or writing fails.
 */
void writeImage(const ImageSpec& spec);

} // namespace image_generator
