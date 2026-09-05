#pragma once

#include "rendering/mesh/MeshScalarGrid.h"

#include <cstdint>
#include <optional>
#include <unordered_map>
#include <vector>

class Image;

namespace rendering::mesh
{

struct SegmentationLabelBounds
{
  glm::uvec3 minVoxel{0u}; //!< Inclusive minimum source voxel containing the label
  glm::uvec3 maxVoxel{0u}; //!< Inclusive maximum source voxel containing the label
};

using SegmentationLabelInventory = std::unordered_map<int64_t, SegmentationLabelBounds>;

/**
 * @brief Exact integer labels packed into consecutive, exactly representable scalar-grid values
 *
 * `labelValues[i]` is represented by scalar value `i + 1` in `grid`. Zero is reserved for background.
 */
struct PackedSegmentationGrid
{
  ScalarGrid3D grid;
  std::vector<int64_t> labelValues;
};

/**
 * @brief Collect the exact label values present in one segmentation time point.
 *
 * The image is scanned once using its native integer representation. Callers can cache the result by pixel-data
 * revision and time point to avoid launching extraction jobs for labels that have no voxels.
 *
 * @return Present labels, or empty when the requested image component/time point cannot be read.
 */
std::optional<SegmentationLabelInventory>
segmentationLabelInventory(const Image& image, uint32_t component, uint32_t timePoint = 0);

/**
 * @brief Convert one Entropy image component into a scalar grid for mesh extraction
 * @param image Source image
 * @param component Logical image component
 * @param timePoint Time frame
 * @param coordinateSpace Coordinate space assigned to extracted mesh positions
 * @return Scalar grid, or empty when the image cannot be converted for extraction
 */
std::optional<ScalarGrid3D> scalarGridFromImageComponent(
  const Image& image,
  uint32_t component,
  uint32_t timePoint = 0,
  MeshCoordinateSpace coordinateSpace = MeshCoordinateSpace::ImageSubject);

/**
 * @brief Convert an image label into an exact binary mask for discrete surface extraction
 *
 * Label comparison is performed against the image's native integer value before conversion to the
 * floating-point scalar-grid representation. This avoids aliasing distinct 32-bit labels that cannot
 * be represented exactly by a float. Only the supplied label bounds plus a one-voxel background border are allocated.
 */
std::optional<ScalarGrid3D> labelMaskGridFromImageComponent(
  const Image& image,
  uint32_t component,
  int64_t labelValue,
  const SegmentationLabelBounds& bounds,
  uint32_t timePoint = 0,
  MeshCoordinateSpace coordinateSpace = MeshCoordinateSpace::ImageSubject);

/**
 * @brief Pack every nonzero label in a segmentation into one shared scalar grid
 *
 * Joint extraction from this representation gives adjacent labels one common boundary. Packing retains exact native
 * integer comparisons while keeping the VTK-independent scalar-grid API.
 */
std::optional<PackedSegmentationGrid> packedSegmentationGridFromImageComponent(
  const Image& image,
  uint32_t component,
  uint32_t timePoint = 0,
  MeshCoordinateSpace coordinateSpace = MeshCoordinateSpace::ImageSubject);

} // namespace rendering::mesh
