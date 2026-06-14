#pragma once

#include <cstdint>
#include <map>

/// Integer type used to identify segmentation labels.
using LabelType = int64_t;

/**
 * @brief Physical distances between voxel-neighbor offsets.
 *
 * The axis distances represent face neighbors, the two-axis distances represent
 * edge neighbors, and @c distXYZ represents corner neighbors.
 */
struct VoxelDistances
{
  float distX; //!< Distance to +/-X neighbors.
  float distY; //!< Distance to +/-Y neighbors.
  float distZ; //!< Distance to +/-Z neighbors.

  float distXY; //!< Distance to diagonal neighbors in the XY plane.
  float distXZ; //!< Distance to diagonal neighbors in the XZ plane.
  float distYZ; //!< Distance to diagonal neighbors in the YZ plane.

  float distXYZ; //!< Distance to diagonal neighbors offset along X, Y, and Z.
};

/// Seed segmentation interpretation.
enum class SeedSegmentationType
{
  Binary,    //!< Seeds are interpreted as foreground/background labels.
  MultiLabel //!< Seeds preserve multiple label identities.
};

/**
 * @brief Bidirectional mapping between segmentation labels and dense indices.
 */
struct LabelIndexMaps
{
  /// Map from segmentation label to label index
  std::map<LabelType, std::size_t> labelToIndex;

  /// Map from label index to segmentation label
  std::map<std::size_t, LabelType> indexToLabel;
};
