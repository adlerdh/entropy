#ifndef GRAPHCUTS_H
#define GRAPHCUTS_H

#include "common/SegmentationTypes.h"

#include <glm/fwd.hpp>
#include <uuid.h>

#include <functional>

bool graphCutsBinarySegmentation(
  const GraphNeighborhoodType& hoodType,
  double terminalCapacity,
  const LabelType& fgSeedValue,
  const LabelType& bgSeedValue,
  const glm::ivec3& dims,
  const VoxelDistances& voxelDistances,
  std::function<double(int x, int y, int z, int dx, int dy, int dz)> getImageWeight,
  std::function<LabelType(int x, int y, int z)> getSeedValue,
  std::function<void(int x, int y, int z, LabelType value)> setResultSegValue
);

bool graphCutsMultiLabelSegmentation(
  const GraphNeighborhoodType& hoodType,
  double terminalCapacity,
  const glm::ivec3& dims,
  const VoxelDistances& voxelDistances,
  std::function<double(int x, int y, int z, int dx, int dy, int dz)> getImageWeight,
  std::function<double(int index1, int index2)> getImageWeight1D,
  std::function<LabelType(int x, int y, int z)> getSeedValue,
  std::function<void(int x, int y, int z, LabelType value)> setResultSegValue
);

#endif // GRAPHCUTS_H
