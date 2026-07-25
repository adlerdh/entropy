#include "rendering/mesh/MeshImageAdapter.h"

#include "image/Image.h"
#include "image/ImageHeader.h"

#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>

#include <limits>

namespace rendering::mesh
{

std::optional<ScalarGrid3D> scalarGridFromImageComponent(
  const Image& image,
  const uint32_t component,
  const uint32_t timePoint,
  const MeshCoordinateSpace coordinateSpace)
{
  const ImageHeader& header = image.header();
  const glm::uvec3& dimensions = header.pixelDimensions();

  if (!image.hasPixelData() || component >= header.numComponentsPerPixel()) {
    return std::nullopt;
  }

  if (dimensions.x < 2 || dimensions.y < 2 || dimensions.z < 2) {
    return std::nullopt;
  }

  if (header.numPixels() > static_cast<uint64_t>(std::numeric_limits<std::size_t>::max())) {
    return std::nullopt;
  }

  ScalarGrid3D grid;
  grid.dimensions = dimensions;
  grid.coordinateSpace = coordinateSpace;
  grid.values.resize(static_cast<std::size_t>(header.numPixels()));

  for (uint32_t k = 0; k < dimensions.z; ++k) {
    for (uint32_t j = 0; j < dimensions.y; ++j) {
      for (uint32_t i = 0; i < dimensions.x; ++i) {
        const std::optional<float> value =
          image.value<float>(component, static_cast<int>(i), static_cast<int>(j), static_cast<int>(k), timePoint);
        if (!value) {
          return std::nullopt;
        }
        grid.values[scalarGridValueIndex(dimensions, i, j, k)] = *value;
      }
    }
  }

  const glm::mat3 voxelBasis = header.directions() * glm::mat3{
                                                       glm::vec3{header.spacing().x, 0.0f, 0.0f},
                                                       glm::vec3{0.0f, header.spacing().y, 0.0f},
                                                       glm::vec3{0.0f, 0.0f, header.spacing().z}};

  grid.grid_T_voxelIndex = glm::mat4{
    glm::vec4{voxelBasis[0], 0.0f},
    glm::vec4{voxelBasis[1], 0.0f},
    glm::vec4{voxelBasis[2], 0.0f},
    glm::vec4{header.origin(), 1.0f}};

  return grid;
}

} // namespace rendering::mesh
