#pragma once

#include "common/SegmentationTypes.h"

#include <glm/fwd.hpp>

#include <functional>

/**
 * @brief Build maps between segmentation label values and compact label indices
 * @param dims Segmentation dimensions in voxels
 * @param getSeedValue Callback returning the label value at voxel `(x, y, z)`
 * @param ignoreBackgroundZeroLabel True to exclude label value zero from the maps
 * @return Bidirectional maps between label values and compact indices
 * @throw Propagates exceptions from `getSeedValue` or map allocation
 */
LabelIndexMaps createLabelIndexMaps(
  const glm::ivec3& dims,
  const std::function<LabelType(int x, int y, int z)>& getSeedValue,
  bool ignoreBackgroundZeroLabel);

/**
 * @brief Compute voxel edge, face-diagonal, and body-diagonal distances
 * @param spacing Physical voxel spacing along i, j, and k
 * @param normalized True to divide all distances by the body diagonal
 * @return Distances used by segmentation neighborhood calculations
 */
VoxelDistances computeVoxelDistances(const glm::vec3& spacing, bool normalized) noexcept;

/**
 * @brief Replace segmentation label values with their compact label indices in place
 * @param segLabels Mutable segmentation label buffer
 * @param dims Segmentation dimensions in voxels
 * @param labelMaps Maps from label values to compact indices
 * @throw Propagates exceptions from label map lookup
 */
void remapSegLabelsToIndices(uint8_t* segLabels, const glm::ivec3& dims, const LabelIndexMaps& labelMaps);

/**
 * @brief Replace compact label indices with their original segmentation label values in place
 * @param segIndices Mutable segmentation index buffer
 * @param dims Segmentation dimensions in voxels
 * @param labelMaps Maps from compact indices to label values
 * @throw Propagates exceptions from label map lookup
 */
void remapSegIndicesToLabels(uint8_t* segIndices, const glm::ivec3& dims, const LabelIndexMaps& labelMaps);
