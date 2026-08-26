#include "rendering/mesh/MeshScalarGrid.h"

#include <limits>

namespace rendering::mesh
{

bool isValidScalarGrid(const ScalarGrid3D& grid)
{
  if (grid.dimensions.x < 2 || grid.dimensions.y < 2 || grid.dimensions.z < 2) {
    return false;
  }

  std::size_t expectedSize = grid.dimensions.x;
  if (grid.dimensions.y > std::numeric_limits<std::size_t>::max() / expectedSize) {
    return false;
  }
  expectedSize *= grid.dimensions.y;
  if (grid.dimensions.z > std::numeric_limits<std::size_t>::max() / expectedSize) {
    return false;
  }
  expectedSize *= grid.dimensions.z;
  return grid.values.size() == expectedSize;
}

std::size_t scalarGridValueIndex(const glm::uvec3& dimensions, uint32_t i, uint32_t j, uint32_t k)
{
  return static_cast<std::size_t>(i) + static_cast<std::size_t>(dimensions.x) *
                                         (static_cast<std::size_t>(j) + static_cast<std::size_t>(dimensions.y) * k);
}

} // namespace rendering::mesh
