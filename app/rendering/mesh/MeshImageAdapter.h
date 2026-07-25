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

} // namespace rendering::mesh
