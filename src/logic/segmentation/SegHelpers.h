#ifndef SEGMENTATION_HELPERS_H
#define SEGMENTATION_HELPERS_H

#include "common/SegmentationTypes.h"

#include <glm/fwd.hpp>

#include <functional>

/**
 * @brief Compute the number of non-zero labels in a segmentation
 */
LabelIndexMaps createLabelIndexMaps(
  const glm::ivec3& dims,
  std::function<LabelType(int x, int y, int z)> getSeedValue,
  bool ignoreBackgroundZeroLabel
);

VoxelDistances computeVoxelDistances(const glm::vec3& spacing, bool normalized);

void remapSegLabelsToIndices(
  uint8_t* segLabels, const glm::ivec3& dims, const LabelIndexMaps& labelMaps
);

void remapSegIndicesToLabels(
  uint8_t* segIndices, const glm::ivec3& dims, const LabelIndexMaps& labelMaps
);

#endif // SEGMENTATION_HELPERS_H
