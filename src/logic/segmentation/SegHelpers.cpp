#include "logic/segmentation/SegHelpers.h"

#include <glm/glm.hpp>

LabelIndexMaps createLabelIndexMaps(
  const glm::ivec3& dims,
  std::function<LabelType(int x, int y, int z)> getSeedValue,
  bool ignoreBackgroundZeroLabel
)
{
  LabelIndexMaps labelMaps;

  std::size_t labelIndex = 0;

  for (int z = 0; z < dims.z; ++z)
  {
    for (int y = 0; y < dims.y; ++y)
    {
      for (int x = 0; x < dims.x; ++x)
      {
        const LabelType label = getSeedValue(x, y, z);

        // Ignore the background (0) label if ignoreBackgroundZeroLabel is true
        if (0 < label || (0 == label && !ignoreBackgroundZeroLabel))
        {
          const auto [iter, inserted] = labelMaps.labelToIndex.emplace(label, labelIndex);

          if (inserted)
          {
            labelMaps.indexToLabel.emplace(labelIndex++, label);
          }
        }
      }
    }
  }

  return labelMaps;
}

VoxelDistances computeVoxelDistances(const glm::vec3& spacing, bool normalized)
{
  VoxelDistances v;

  const double L = (normalized) ? glm::length(spacing) : 1.0f;

  v.distXYZ = glm::length(spacing) / L;

  v.distX = glm::length(glm::vec3{spacing.x, 0, 0}) / L;
  v.distY = glm::length(glm::vec3{0, spacing.y, 0}) / L;
  v.distZ = glm::length(glm::vec3{0, 0, spacing.z}) / L;

  v.distXY = glm::length(glm::vec3{spacing.x, spacing.y, 0}) / L;
  v.distXZ = glm::length(glm::vec3{spacing.x, 0, spacing.z}) / L;
  v.distYZ = glm::length(glm::vec3{0, spacing.y, spacing.z}) / L;

  return v;
}
