#pragma once

#include <glm/mat3x3.hpp>
#include <glm/vec3.hpp>

#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

namespace warp_field
{
constexpr std::size_t kMaxSpatialDimension = 3;
constexpr std::size_t kMaxImageDimension = 4;

enum class OperationType
{
  Affine,
  InverseAffine,
  Expansion,
  Contraction,
  Rotation,
  Twist,
  Swirl,
  VortexPair,
  SourceSinkPair,
  Wave,
  Shear,
  Stretch,
  Fold,
  Tear,
  ControlPoint,
  SmoothRandom,
  PiecewiseAffine,
  SlidingInterface,
  DivergenceFree,
  CurlFree,
  JacobianExpansion,
  Landmark,
  BoundaryConstrained,
  Jump,
  WhiteNoise
};

enum class TimeMode
{
  Constant,
  Sine,
  Cosine,
  Ramp
};

enum class CompositionMode
{
  Additive,
  Composed
};

/**
 * @brief Time modulation applied to one warp operation.
 */
struct TimeProfile
{
  TimeMode mode = TimeMode::Constant; //!< Function used to scale the operation over time
  double amplitude = 1.0; //!< Multiplier applied to the time profile
  double frequency = 1.0; //!< Cycles across normalized time for periodic profiles
  double phase = 0.0; //!< Phase offset in radians for periodic profiles
  double offset = 0.0; //!< Additive offset applied after the profile value
};

/**
 * @brief One displacement primitive in a synthetic warp field.
 */
struct WarpOperation
{
  OperationType type = OperationType::Affine; //!< Displacement primitive to evaluate
  TimeProfile time; //!< Optional time modulation
  glm::dvec3 center{0.0, 0.0, 0.0}; //!< Physical center used by localized operations
  glm::dvec3 secondaryCenter{20.0, 0.0, 0.0}; //!< Second center used by paired operations
  glm::dvec3 radii{25.0, 25.0, 25.0}; //!< Ellipsoid radii in physical units
  glm::dvec3 direction{1.0, 0.0, 0.0}; //!< Displacement, wave, fold, or tear direction
  glm::dvec3 secondaryDirection{-1.0, 0.0, 0.0}; //!< Second displacement direction used by paired operations
  glm::dvec3 axis{0.0, 0.0, 1.0}; //!< Twist axis for 3D fields
  glm::dvec3 translation{0.0, 0.0, 0.0}; //!< Translation component for affine fields
  glm::dvec3 secondaryTranslation{0.0, 0.0, 0.0}; //!< Translation on the far side of piecewise affine fields
  glm::dvec3 scale{1.0, 1.0, 1.0}; //!< Component-wise scale used by stretch fields
  glm::dvec3 normal{1.0, 0.0, 0.0}; //!< Plane normal used by waves, folds, and tears
  glm::dmat3 matrix{1.0}; //!< Linear part of affine fields
  glm::dmat3 secondaryMatrix{1.0}; //!< Second linear transform
  double amplitude = 1.0; //!< Operation strength in physical units or radians
  double frequency = 1.0; //!< Spatial frequency for waves
  double phase = 0.0; //!< Spatial phase in radians for waves
  double width = 1.0; //!< Transition width for folds and tears
  double cellSize = 8.0; //!< Spatial scale for procedural random fields
  unsigned int seed = 1; //!< Seed for procedural random fields
  std::vector<glm::dvec3> controlPoints; //!< Control point positions or fixed landmarks
  std::vector<glm::dvec3> displacements; //!< Control point displacements or moving-fixed offsets
  std::vector<double> weights; //!< Optional per-control-point radii or weights
};

/**
 * @brief Complete synthetic warp field description.
 */
struct WarpFieldSpec
{
  std::filesystem::path output; //!< Destination image file
  std::string componentType = "float"; //!< ITK component type to write
  std::size_t spatialDimension = 3; //!< Spatial field dimension, 1 through 3
  std::size_t vectorDimension = 3; //!< Number of displacement components per voxel
  std::vector<std::size_t> size{64, 64, 64}; //!< Image size, plus optional time size
  std::vector<double> spacing{1.0, 1.0, 1.0}; //!< Image spacing, plus optional time spacing
  std::vector<double> origin{0.0, 0.0, 0.0}; //!< Image origin, plus optional time origin
  std::vector<double> direction; //!< Row-major image direction matrix
  CompositionMode compositionMode = CompositionMode::Additive; //!< How operations are combined
  std::vector<WarpOperation> operations; //!< Operations summed into the final field
};

/**
 * @brief Parse a warp field specification from JSON text.
 * @param text JSON document to parse.
 * @return Parsed specification.
 * @throws std::runtime_error when the JSON is invalid or incomplete.
 */
WarpFieldSpec parseSpecJson(const std::string& text);

/**
 * @brief Read and parse a warp field specification from disk.
 * @param fileName JSON specification file.
 * @return Parsed specification.
 * @throws std::runtime_error when the file cannot be read or parsed.
 */
WarpFieldSpec loadSpecJson(const std::filesystem::path& fileName);

/**
 * @brief Validate a warp field specification.
 * @param spec Specification to validate.
 * @throws std::runtime_error when the specification is inconsistent.
 */
void validateSpec(const WarpFieldSpec& spec);

/**
 * @brief Evaluate the displacement at one physical point and normalized time.
 * @param spec Warp field specification.
 * @param point Physical spatial point.
 * @param normalizedTime Time in [0, 1] for time-dependent fields.
 * @return Displacement vector.
 */
glm::dvec3 evaluateDisplacement(const WarpFieldSpec& spec, const glm::dvec3& point, double normalizedTime);

/**
 * @brief Write a synthetic warp field image to disk.
 * @param spec Specification to generate and write.
 * @throws std::runtime_error when the field cannot be generated or written.
 */
void writeWarpField(const WarpFieldSpec& spec);

/**
 * @brief Normalize a 3D vector, preserving zero vectors.
 * @param vector Input vector.
 * @return Unit vector, or zero when the input vector has zero length.
 */
glm::dvec3 normalized(const glm::dvec3& vector);

} // namespace warp_field
