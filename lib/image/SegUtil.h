#pragma once

#include "common/Types.h"

#include <glm/fwd.hpp>
#include <glm/vec3.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>

#include <uuid.h>
#include <functional>
#include <unordered_set>

class Image;

using SegmentationVoxelSet = std::unordered_set<glm::ivec3>;

/**
 * @brief paintSegmentation
 * @param seg
 * @param labelToPaint
 * @param labelToReplace
 * @param brushReplacesBgWithFg
 * @param brushIsRound
 * @param brushIs3d
 * @param brushIsIsotropic
 * @param brushSizeInVoxels
 * @param roundedPixelPos
 * @param voxelViewPlane
 * @param updateSegTexture
 */
void paintSegmentation(
  Image& seg,

  int64_t labelToPaint,
  int64_t labelToReplace,

  bool brushReplacesBgWithFg,
  bool brushIsRound,
  bool brushIs3d,
  bool brushIsIsotropic,
  int brushSizeInVoxels,

  const glm::ivec3& roundedPixelPos,
  const glm::vec4& voxelViewPlane,

  const std::function<void(
    const ComponentType& memoryComponentType,
    const glm::uvec3& offset,
    const glm::uvec3& size,
    const int64_t* data)>& updateSegTexture);

void updateSegmentationVoxels(
  const SegmentationVoxelSet& voxelsToChange,
  const glm::ivec3& minVoxel,
  const glm::ivec3& maxVoxel,

  int64_t labelToPaint,
  int64_t labelToReplace,
  bool brushReplacesBgWithFg,

  Image& seg,

  const std::function<void(
    const ComponentType& memoryComponentType,
    const glm::uvec3& offset,
    const glm::uvec3& size,
    const int64_t* data)>& updateSegTexture);
