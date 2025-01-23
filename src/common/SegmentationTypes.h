#ifndef SEGMENTATION_TYPES_H
#define SEGMENTATION_TYPES_H

#include <cstdint>
#include <map>

using LabelType = int64_t;

// Distances between voxel neighbors
struct VoxelDistances
{
  float distX;
  float distY;
  float distZ;

  float distXY;
  float distXZ;
  float distYZ;

  float distXYZ;
};

enum class SeedSegmentationType
{
  Binary,
  MultiLabel
};

enum class GraphNeighborhoodType
{
  Neighbors6, // 6 face neighbors
  Neighbors26 // 26 face, edge, and vertex neighbors
};

struct LabelIndexMaps
{
  /// Map from segmentation label to label index
  std::map<LabelType, std::size_t> labelToIndex;

  /// Map from label index to segmentation label
  std::map<std::size_t, LabelType> indexToLabel;
};

#endif // SEGMENTATION_TYPES_H
