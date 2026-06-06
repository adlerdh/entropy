#pragma once

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

struct LabelIndexMaps
{
  /// Map from segmentation label to label index
  std::map<LabelType, std::size_t> labelToIndex;

  /// Map from label index to segmentation label
  std::map<std::size_t, LabelType> indexToLabel;
};
