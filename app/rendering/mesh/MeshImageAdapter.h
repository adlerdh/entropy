#pragma once

#include "rendering/mesh/MeshScalarGrid.h"

#include <cstdint>
#include <optional>

class Image;

namespace rendering::mesh
{

/**
 * @brief Convert one Entropy image component into a scalar grid for mesh extraction
 * @param image Source image
 * @param component Logical image component
 * @param timePoint Time frame
 * @param coordinateSpace Coordinate space assigned to extracted mesh positions
 * @return Scalar grid, or empty when the image cannot be converted for extraction
 */
std::optional<ScalarGrid3D> scalarGridFromImageComponent(
  const Image& image,
  uint32_t component,
  uint32_t timePoint = 0,
  MeshCoordinateSpace coordinateSpace = MeshCoordinateSpace::ImageSubject);

/**
 * @brief Convert an image label into an exact binary mask for discrete surface extraction
 *
 * Label comparison is performed against the image's native integer value before conversion to the
 * floating-point scalar-grid representation. This avoids aliasing distinct 32-bit labels that cannot
 * be represented exactly by a float.
 */
std::optional<ScalarGrid3D> labelMaskGridFromImageComponent(
  const Image& image,
  uint32_t component,
  int64_t labelValue,
  uint32_t timePoint = 0,
  MeshCoordinateSpace coordinateSpace = MeshCoordinateSpace::ImageSubject);

} // namespace rendering::mesh
