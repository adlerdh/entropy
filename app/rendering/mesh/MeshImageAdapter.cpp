#include "rendering/mesh/MeshImageAdapter.h"

#include "image/Image.h"
#include "image/ImageHeader.h"
#include "image/ImageTransformations.h"

#include <glm/mat4x4.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/vec4.hpp>

#include <algorithm>
#include <cstddef>
#include <limits>
#include <unordered_map>

namespace rendering::mesh
{

namespace
{

std::optional<ScalarGrid3D>
initializedGrid(const Image& image, const uint32_t component, const MeshCoordinateSpace coordinateSpace)
{
  const ImageHeader& header = image.header();
  const glm::uvec3& dimensions = header.pixelDimensions();

  if (
    !image.hasPixelData() || component >= header.numComponentsPerPixel() || dimensions.x < 2 || dimensions.y < 2 ||
    dimensions.z < 2 || glm::any(glm::greaterThan(dimensions, glm::uvec3{std::numeric_limits<int>::max()})) ||
    header.numPixels() > static_cast<uint64_t>(std::numeric_limits<std::size_t>::max()))
  {
    return std::nullopt;
  }

  ScalarGrid3D grid;
  grid.dimensions = dimensions;
  grid.coordinateSpace = coordinateSpace;
  grid.values.resize(static_cast<std::size_t>(header.numPixels()));
  grid.grid_T_voxelIndex = MeshCoordinateSpace::World == coordinateSpace ? image.transformations().worldDef_T_pixel()
                                                                         : image.transformations().subject_T_pixel();
  return grid;
}

} // namespace

std::optional<SegmentationLabelInventory>
segmentationLabelInventory(const Image& image, const uint32_t component, const uint32_t timePoint)
{
  if (!image.hasPixelData() || component >= image.header().numComponentsPerPixel()) {
    return std::nullopt;
  }

  SegmentationLabelInventory labels;
  const glm::uvec3 dimensions = image.header().pixelDimensions();
  if (glm::any(glm::greaterThan(dimensions, glm::uvec3{std::numeric_limits<int>::max()}))) {
    return std::nullopt;
  }
  for (uint32_t k = 0; k < dimensions.z; ++k) {
    for (uint32_t j = 0; j < dimensions.y; ++j) {
      for (uint32_t i = 0; i < dimensions.x; ++i) {
        const std::optional<int64_t> value =
          image.value<int64_t>(component, static_cast<int>(i), static_cast<int>(j), static_cast<int>(k), timePoint);
        if (!value) {
          return std::nullopt;
        }
        const glm::uvec3 voxel{i, j, k};
        auto [it, inserted] = labels.try_emplace(*value, SegmentationLabelBounds{.minVoxel = voxel, .maxVoxel = voxel});
        if (!inserted) {
          it->second.minVoxel = glm::min(it->second.minVoxel, voxel);
          it->second.maxVoxel = glm::max(it->second.maxVoxel, voxel);
        }
      }
    }
  }
  return labels;
}

std::optional<ScalarGrid3D> scalarGridFromImageComponent(
  const Image& image,
  const uint32_t component,
  const uint32_t timePoint,
  const MeshCoordinateSpace coordinateSpace)
{
  std::optional<ScalarGrid3D> result = initializedGrid(image, component, coordinateSpace);
  if (!result) {
    return std::nullopt;
  }
  ScalarGrid3D& grid = *result;
  const glm::uvec3& dimensions = grid.dimensions;

  for (uint32_t k = 0; k < dimensions.z; ++k) {
    for (uint32_t j = 0; j < dimensions.y; ++j) {
      for (uint32_t i = 0; i < dimensions.x; ++i) {
        const std::optional<float> value =
          image.value<float>(component, static_cast<int>(i), static_cast<int>(j), static_cast<int>(k), timePoint);
        if (!value) {
          return std::nullopt;
        }
        grid.values[scalarGridValueIndex(dimensions, i, j, k)] = *value;
      }
    }
  }

  return result;
}

std::optional<ScalarGrid3D> labelMaskGridFromImageComponent(
  const Image& image,
  const uint32_t component,
  const int64_t labelValue,
  const SegmentationLabelBounds& bounds,
  const uint32_t timePoint,
  const MeshCoordinateSpace coordinateSpace)
{
  const glm::uvec3 imageDimensions = image.header().pixelDimensions();
  if (
    !image.hasPixelData() || component >= image.header().numComponentsPerPixel() ||
    glm::any(glm::greaterThan(bounds.minVoxel, bounds.maxVoxel)) ||
    glm::any(glm::greaterThanEqual(bounds.maxVoxel, imageDimensions)))
  {
    return std::nullopt;
  }

  const glm::uvec3 occupiedSize = bounds.maxVoxel - bounds.minVoxel + glm::uvec3{1u};
  if (glm::any(glm::greaterThan(occupiedSize, glm::uvec3{std::numeric_limits<uint32_t>::max() - 2u}))) {
    return std::nullopt;
  }

  ScalarGrid3D grid;
  grid.dimensions = occupiedSize + glm::uvec3{2u};
  grid.coordinateSpace = coordinateSpace;
  const std::size_t voxelCount = static_cast<std::size_t>(grid.dimensions.x) * grid.dimensions.y * grid.dimensions.z;
  grid.values.assign(voxelCount, 0.0f);
  grid.grid_T_voxelIndex = (MeshCoordinateSpace::World == coordinateSpace ? image.transformations().worldDef_T_pixel()
                                                                          : image.transformations().subject_T_pixel()) *
                           glm::translate(glm::mat4{1.0f}, glm::vec3{bounds.minVoxel} - glm::vec3{1.0f});

  for (uint32_t z = 0; z < occupiedSize.z; ++z) {
    for (uint32_t y = 0; y < occupiedSize.y; ++y) {
      for (uint32_t x = 0; x < occupiedSize.x; ++x) {
        const glm::uvec3 sourceVoxel = bounds.minVoxel + glm::uvec3{x, y, z};
        const std::optional<int64_t> value = image.value<int64_t>(
          component,
          static_cast<int>(sourceVoxel.x),
          static_cast<int>(sourceVoxel.y),
          static_cast<int>(sourceVoxel.z),
          timePoint);
        if (!value) {
          return std::nullopt;
        }
        const glm::uvec3 croppedVoxel = glm::uvec3{x, y, z} + glm::uvec3{1u};
        grid.values[scalarGridValueIndex(grid.dimensions, croppedVoxel.x, croppedVoxel.y, croppedVoxel.z)] =
          *value == labelValue ? 1.0f : 0.0f;
      }
    }
  }
  return grid;
}

std::optional<PackedSegmentationGrid> packedSegmentationGridFromImageComponent(
  const Image& image,
  const uint32_t component,
  const uint32_t timePoint,
  const MeshCoordinateSpace coordinateSpace)
{
  const std::optional<SegmentationLabelInventory> inventory = segmentationLabelInventory(image, component, timePoint);
  if (!inventory) {
    return std::nullopt;
  }

  PackedSegmentationGrid packed;
  packed.labelValues.reserve(inventory->size());
  for (const auto& [labelValue, bounds] : *inventory) {
    static_cast<void>(bounds);
    if (labelValue != 0) {
      packed.labelValues.push_back(labelValue);
    }
  }
  std::ranges::sort(packed.labelValues);
  if (packed.labelValues.empty() || packed.labelValues.size() > 16'777'215u) {
    return std::nullopt;
  }

  std::unordered_map<int64_t, float> packedValues;
  packedValues.reserve(packed.labelValues.size());
  for (std::size_t index = 0; index < packed.labelValues.size(); ++index) {
    packedValues.emplace(packed.labelValues[index], static_cast<float>(index + 1u));
  }

  const glm::uvec3 imageDimensions = image.header().pixelDimensions();
  if (glm::any(glm::greaterThan(imageDimensions, glm::uvec3{std::numeric_limits<uint32_t>::max() - 2u}))) {
    return std::nullopt;
  }
  packed.grid.dimensions = imageDimensions + glm::uvec3{2u};
  packed.grid.coordinateSpace = coordinateSpace;
  const std::size_t width = packed.grid.dimensions.x;
  const std::size_t height = packed.grid.dimensions.y;
  const std::size_t depth = packed.grid.dimensions.z;
  if (
    height > std::numeric_limits<std::size_t>::max() / width ||
    depth > std::numeric_limits<std::size_t>::max() / (width * height))
  {
    return std::nullopt;
  }
  const std::size_t voxelCount = width * height * depth;
  packed.grid.values.assign(voxelCount, 0.0f);
  packed.grid.grid_T_voxelIndex =
    (MeshCoordinateSpace::World == coordinateSpace ? image.transformations().worldDef_T_pixel()
                                                   : image.transformations().subject_T_pixel()) *
    glm::translate(glm::mat4{1.0f}, glm::vec3{-1.0f});

  for (uint32_t z = 0; z < imageDimensions.z; ++z) {
    for (uint32_t y = 0; y < imageDimensions.y; ++y) {
      for (uint32_t x = 0; x < imageDimensions.x; ++x) {
        const std::optional<int64_t> value =
          image.value<int64_t>(component, static_cast<int>(x), static_cast<int>(y), static_cast<int>(z), timePoint);
        if (!value) {
          return std::nullopt;
        }
        const auto packedValue = packedValues.find(*value);
        if (packedValue == packedValues.end()) {
          continue;
        }
        const glm::uvec3 destination = glm::uvec3{x, y, z} + glm::uvec3{1u};
        packed.grid.values[scalarGridValueIndex(packed.grid.dimensions, destination.x, destination.y, destination.z)] =
          packedValue->second;
      }
    }
  }

  return packed;
}

} // namespace rendering::mesh
