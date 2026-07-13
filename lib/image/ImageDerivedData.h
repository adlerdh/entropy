#pragma once

#include "common/Expected.h"
#include "image/Image.h"

#include <cstdint>
#include <glm/vec3.hpp>
#include <optional>
#include <string>
#include <vector>

/**
 * @brief Scalar projections computed across all components of a multi-component image.
 */
enum class ComponentProjectionMode
{
  Minimum,
  Mean,
  Maximum,
  Magnitude,
  ComplexPhaseSignedRadians,
  ComplexPhaseUnsignedRadians,
  ComplexPhaseSignedDegrees,
  ComplexPhaseUnsignedDegrees,
  VectorJacobianDeterminant,
  VectorLogJacobianDeterminant,
  VectorGradientMagnitude,
  VectorDivergence,
  VectorCurlMagnitude,
  VectorLaplacianMagnitude
};

/**
 * @brief Return the scalar projection needed by a multi-component render mode.
 * @param mode Multi-component render mode.
 * @return Projection mode for scalar projections, or std::nullopt for modes rendered directly.
 */
std::optional<ComponentProjectionMode> componentProjectionFromRenderMode(ComponentRenderMode mode);

/**
 * @brief Return the scalar projection needed by the image's current render settings.
 * @param image Image whose settings select the render mode.
 * @return Projection mode for scalar projections, or std::nullopt for modes rendered directly.
 */
std::optional<ComponentProjectionMode> componentProjectionForImage(const Image& image);

/**
 * @brief Return the scalar projection for a complex phase display convention.
 * @param range Phase range convention.
 * @param unit Phase display unit.
 * @return Projection mode for the selected phase display convention.
 */
ComponentProjectionMode complexPhaseProjectionMode(ComplexPhaseRange range, ComplexPhaseUnit unit);

/**
 * @brief Get a short UI label for a component projection.
 * @param mode Projection mode.
 * @return Human-readable projection name.
 */
std::string componentProjectionModeName(ComponentProjectionMode mode);

/**
 * @brief Return whether a projection mode computes a scalar image from image components.
 * @param mode Projection mode.
 * @return True for modes that require a derived scalar projection image.
 */
bool isScalarComponentProjection(ComponentProjectionMode mode);

/**
 * @brief Compute a vector-field derivative projection at one voxel.
 * @param image Three-component vector-field image.
 * @param mode Vector derivative projection to compute.
 * @param voxel Voxel coordinate where the value is sampled.
 * @param timePoint Source image time point.
 * @return Projection value, or std::nullopt when the image, mode, or voxel is invalid.
 */
std::optional<double> vectorDerivativeProjectionValue(
  const Image& image,
  ComponentProjectionMode mode,
  const glm::uvec3& voxel,
  uint32_t timePoint = 0);

/**
 * @brief Return whether the image is a two-component complex-valued image.
 * @param image Image to inspect.
 * @return True when metadata identifies the image as complex and it has real/imaginary components.
 */
bool isComplexValuedImage(const Image& image);

/**
 * @brief Return whether an image can reasonably be interpreted as a 3D vector field.
 * @param image Image to inspect.
 * @return True for loaded 3-component images.
 */
bool isVectorFieldCandidate(const Image& image);

/**
 * @brief Return whether an image is currently treated as a vector field.
 * @param image Image to inspect.
 * @return True when vector-field rendering controls are enabled for a candidate image.
 */
bool isVectorFieldImage(const Image& image);

/**
 * @brief Compute the phase angle of a complex value.
 * @param real Real component.
 * @param imaginary Imaginary component.
 * @param range Phase range convention.
 * @param unit Phase display unit.
 * @return Phase in the requested range and unit.
 */
double complexPhaseValue(double real, double imaginary, ComplexPhaseRange range, ComplexPhaseUnit unit);

/**
 * @brief Label for a complex image component index.
 * @param component Component index.
 * @param componentMax Maximum valid component index.
 * @return Human-readable label including real/imaginary semantics when known.
 */
std::string complexComponentLabel(uint32_t component, uint32_t componentMax);

/**
 * @brief Label for a vector-field component index.
 * @param component Component index.
 * @param componentMax Maximum valid component index.
 * @return Human-readable label including x/y/z semantics when known.
 */
std::string vectorFieldComponentLabel(uint32_t component, uint32_t componentMax);

/**
 * @brief Derived image produced for one source image component.
 */
struct ComponentImageResult
{
  uint32_t component{0}; //!< Source image component used to compute the derived image
  Image image;           //!< Derived scalar image
};

/**
 * @brief Distance-map image produced for one source image component.
 */
struct DistanceMapImageResult
{
  uint32_t component{0};        //!< Source image component used to compute the distance map
  Image image;                  //!< Derived scalar distance-map image
  double boundaryIsoValue{0.0}; //!< Isovalue corresponding to the foreground boundary in the distance map
};

/**
 * @brief Create one noise-estimate image for each supported component of an image.
 * @param image Source image.
 * @param radius Neighborhood radius, in voxels, used by the noise-estimation filter.
 * @return Derived images paired with their source component indices.
 */
std::vector<ComponentImageResult> createNoiseEstimateImages(const Image& image, uint32_t radius);

/**
 * @brief Create Euclidean distance maps for supported components of an image.
 * @param image Source image.
 * @param downsamplingFactor Factor applied before distance-map computation.
 * @return Distance-map images paired with their source component indices and boundary isovalues.
 */
std::vector<DistanceMapImageResult> createDistanceMapImages(const Image& image, float downsamplingFactor);

/**
 * @brief Create a scalar image by projecting each pixel's component vector.
 * @param image Source multi-component image.
 * @param mode Projection to compute across components at each pixel.
 * @param timePoint Source image time point.
 * @return Derived Float32 scalar image, or an error explaining why it cannot be created.
 */
entropy_expected::expected<Image, std::string>
createComponentProjectionImage(const Image& image, ComponentProjectionMode mode, uint32_t timePoint = 0);
