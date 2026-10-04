#include "deformation/FieldTiles.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>

namespace deformation
{
std::optional<IndexExtent>
compositionDependencies(SampledFieldView inner, const IndexExtent& outputTile, std::size_t maxSamples)
{
  const auto& domain = inner.domain;
  if (inner.values.size() != domain.sampleCount()) throw std::invalid_argument("Inner field sample count mismatch");
  for (int axis = 0; axis < 3; ++axis) {
    if (outputTile.begin[axis] >= outputTile.end[axis] || outputTile.end[axis] > domain.size()[axis])
      throw std::invalid_argument("Composition tile is outside the field domain");
  }
  const auto& valid = domain.validExtent();
  const int dimensions = domain.dimension() == SpatialDimension::Plane ? 2 : 3;
  IndexExtent needed{{domain.size()[0], domain.size()[1], domain.size()[2]}, {0, 0, 0}};
  bool any = false;
  for (std::uint32_t z = outputTile.begin[2]; z < outputTile.end[2]; ++z) {
    for (std::uint32_t y = outputTile.begin[1]; y < outputTile.end[1]; ++y) {
      for (std::uint32_t x = outputTile.begin[0]; x < outputTile.end[0]; ++x) {
        const glm::dvec3 index(x, y, z);
        const auto point = domain.indexToPhysical(index);
        const auto displacement = sampleDisplacement(inner, point);
        if (!displacement) continue;
        const auto displaced = domain.physicalToIndex(point + *displacement);
        bool covered = true;
        for (int axis = 0; axis < dimensions; ++axis) {
          if (
            displaced[axis] < static_cast<double>(valid.begin[axis]) - 1e-8 ||
            displaced[axis] > static_cast<double>(valid.end[axis] - 1) + 1e-8)
            covered = false;
        }
        if (!covered) continue;
        any = true;
        for (int axis = 0; axis < dimensions; ++axis) {
          const auto q = std::clamp(
            displaced[axis],
            static_cast<double>(valid.begin[axis]),
            static_cast<double>(valid.end[axis] - 1));
          const auto low = static_cast<std::uint32_t>(std::floor(q));
          const auto high = std::min(low + 1, valid.end[axis] - 1);
          needed.begin[axis] = std::min(needed.begin[axis], low > valid.begin[axis] ? low - 1 : low);
          const auto end = high >= valid.end[axis] - 1 ? valid.end[axis] : std::min(high + 2, valid.end[axis]);
          needed.end[axis] = std::max(needed.end[axis], end);
        }
      }
    }
  }
  if (!any) return std::nullopt;
  if (dimensions == 2) {
    needed.begin[2] = 0;
    needed.end[2] = 1;
  }
  std::size_t count = 1;
  for (int axis = 0; axis < 3; ++axis) {
    const auto length = needed.end[axis] - needed.begin[axis];
    if (length > maxSamples / count) throw std::invalid_argument("Composition dependency budget exceeded");
    count *= length;
  }
  return needed;
}
} // namespace deformation
