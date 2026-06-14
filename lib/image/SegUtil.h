#pragma once

#include "common/Types.h"

#include <glm/fwd.hpp>
#include <glm/vec3.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>

#include <cstdint>
#include <functional>
#include <optional>
#include <unordered_set>

class Image;

/// @brief Set of integer voxel coordinates affected by a segmentation operation.
using SegmentationVoxelSet = std::unordered_set<glm::ivec3>;

/**
 * @brief Callback invoked after a contiguous block of segmentation voxels changes.
 *
 * The callback receives the segmentation memory component type, changed block offset and size, and
 * a pointer to changed voxel labels encoded as int64_t values.
 */
using SegmentationVoxelUpdateCallback = std::function<void(
  const ComponentType& memoryComponentType,
  const glm::uvec3& offset,
  const glm::uvec3& size,
  const int64_t* data)>;

/**
 * @brief Voxel footprint of a segmentation brush in image voxel coordinates.
 */
struct SegmentationBrushFootprint
{
  SegmentationVoxelSet voxels; //!< Unique voxels touched by the brush.
  glm::ivec3 minVoxel{0};      //!< Minimum touched voxel coordinate.
  glm::ivec3 maxVoxel{0};      //!< Maximum touched voxel coordinate.
};

/**
 * @brief Compute the set of voxels touched by a segmentation brush.
 * @param seg Segmentation image defining bounds and spacing.
 * @param brushIsRound When true, use an ellipsoidal/circular footprint rather than a box.
 * @param brushIs3d When true, include voxels through-plane; otherwise restrict to the view plane.
 * @param brushIsIsotropic When true, scale the footprint by physical spacing.
 * @param brushSizeInVoxels Diameter-like brush size in voxels.
 * @param roundedPixelPos Center voxel of the brush.
 * @param voxelViewPlane View plane used for 2D brush footprints.
 * @param referenceSpacing Optional spacing used for isotropic brush scaling.
 */
SegmentationBrushFootprint computeSegmentationBrushFootprint(
  const Image& seg,
  bool brushIsRound,
  bool brushIs3d,
  bool brushIsIsotropic,
  int brushSizeInVoxels,
  const glm::ivec3& roundedPixelPos,
  const glm::vec4& voxelViewPlane,
  std::optional<glm::vec3> referenceSpacing = std::nullopt);

/**
 * @brief Paint labels into a segmentation image using a computed brush footprint.
 * @param seg Segmentation image to modify.
 * @param labelToPaint Label written by the brush.
 * @param labelToReplace Label eligible for replacement when brushReplacesBgWithFg is true.
 * @param brushReplacesBgWithFg When true, only voxels with labelToReplace are painted.
 * @param brushIsRound When true, use an ellipsoidal/circular footprint rather than a box.
 * @param brushIs3d When true, include voxels through-plane; otherwise restrict to the view plane.
 * @param brushIsIsotropic When true, scale the footprint by physical spacing.
 * @param brushSizeInVoxels Diameter-like brush size in voxels.
 * @param roundedPixelPos Center voxel of the brush.
 * @param voxelViewPlane View plane used for 2D brush footprints.
 * @param referenceSpacing Optional spacing used for isotropic brush scaling.
 * @param notifyVoxelsChanged Callback receiving the contiguous changed voxel block.
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
  std::optional<glm::vec3> referenceSpacing,

  const SegmentationVoxelUpdateCallback& notifyVoxelsChanged);

/**
 * @brief Apply a precomputed set of segmentation voxel updates.
 * @param voxelsToChange Voxels eligible for update.
 * @param minVoxel Minimum coordinate of voxelsToChange.
 * @param maxVoxel Maximum coordinate of voxelsToChange.
 * @param labelToPaint Label written to changed voxels.
 * @param labelToReplace Label eligible for replacement when brushReplacesBgWithFg is true.
 * @param brushReplacesBgWithFg When true, only voxels with labelToReplace are painted.
 * @param seg Segmentation image to modify.
 * @param notifyVoxelsChanged Callback receiving the contiguous changed voxel block.
 */
void updateSegmentationVoxels(
  const SegmentationVoxelSet& voxelsToChange,
  const glm::ivec3& minVoxel,
  const glm::ivec3& maxVoxel,

  int64_t labelToPaint,
  int64_t labelToReplace,
  bool brushReplacesBgWithFg,

  Image& seg,

  const SegmentationVoxelUpdateCallback& notifyVoxelsChanged);
