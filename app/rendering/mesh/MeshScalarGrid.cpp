#include "rendering/mesh/MeshScalarGrid.h"

namespace rendering::mesh
{

bool isValidScalarGrid(const ScalarGrid3D& grid)
{
  if (grid.dimensions.x < 2 || grid.dimensions.y < 2 || grid.dimensions.z < 2) {
    return false;
  }

  const std::size_t expectedSize = static_cast<std::size_t>(grid.dimensions.x) * grid.dimensions.y * grid.dimensions.z;
  return grid.values.size() == expectedSize;
}

std::size_t scalarGridValueIndex(const glm::uvec3& dimensions, uint32_t i, uint32_t j, uint32_t k)
{
  return static_cast<std::size_t>(i) + static_cast<std::size_t>(dimensions.x) *
                                         (static_cast<std::size_t>(j) + static_cast<std::size_t>(dimensions.y) * k);
}

} // namespace rendering::mesh
