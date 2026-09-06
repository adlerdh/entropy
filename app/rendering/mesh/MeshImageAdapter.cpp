#include "rendering/mesh/MeshImageAdapter.h"

#include "image/Image.h"
#include "image/ImageHeader.h"
#include "image/ImageTransformations.h"

#include <glm/mat4x4.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/vec4.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <ranges>
#include <type_traits>
#include <vector>

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

struct ComponentBufferView
{
  const void* values = nullptr;
  std::size_t pixelStride = 1u;
  std::size_t componentOffset = 0u;
};

std::optional<ComponentBufferView>
componentBufferView(const Image& image, const uint32_t component, const uint32_t timePoint)
{
  if (
    !image.hasPixelData() || component >= image.header().numComponentsPerPixel() ||
    timePoint >= image.timeAxis().numTimePoints())
  {
    return std::nullopt;
  }

  const bool interleaved = image.bufferType() == Image::MultiComponentBufferType::InterleavedImage;
  const void* const values = image.bufferAsVoid(interleaved ? 0u : component, timePoint);
  if (!values) {
    return std::nullopt;
  }
  return ComponentBufferView{
    .values = values,
    .pixelStride = interleaved ? image.header().numComponentsPerPixel() : 1u,
    .componentOffset = interleaved ? component : 0u};
}

template<typename T>
SegmentationLabelInventory
scanSparseLabels(const T* const values, const ComponentBufferView& view, const glm::uvec3 dims)
{
  SegmentationLabelInventory labels;
  const std::size_t width = dims.x;
  const std::size_t sliceSize = width * dims.y;
  for (uint32_t z = 0; z < dims.z; ++z) {
    for (uint32_t y = 0; y < dims.y; ++y) {
      for (uint32_t x = 0; x < dims.x; ++x) {
        const std::size_t index = static_cast<std::size_t>(z) * sliceSize + static_cast<std::size_t>(y) * width + x;
        const int64_t label = static_cast<int64_t>(values[index * view.pixelStride + view.componentOffset]);
        const glm::uvec3 voxel{x, y, z};
        auto [labelIt, inserted] =
          labels.try_emplace(label, SegmentationLabelBounds{.minVoxel = voxel, .maxVoxel = voxel});
        if (!inserted) {
          labelIt->second.minVoxel = glm::min(labelIt->second.minVoxel, voxel);
          labelIt->second.maxVoxel = glm::max(labelIt->second.maxVoxel, voxel);
        }
        const auto recordSharedBoundary = [&labels, label](const int64_t neighbor) {
          if (label == 0 || neighbor == 0 || label == neighbor) {
            return;
          }
          labels.at(label).hasSharedBoundary = true;
          labels.at(neighbor).hasSharedBoundary = true;
        };
        if (x > 0u) {
          recordSharedBoundary(static_cast<int64_t>(values[(index - 1u) * view.pixelStride + view.componentOffset]));
        }
        if (y > 0u) {
          recordSharedBoundary(static_cast<int64_t>(values[(index - width) * view.pixelStride + view.componentOffset]));
        }
        if (z > 0u) {
          recordSharedBoundary(
            static_cast<int64_t>(values[(index - sliceSize) * view.pixelStride + view.componentOffset]));
        }
      }
    }
  }
  return labels;
}

template<typename T>
  requires(std::is_unsigned_v<T> && sizeof(T) <= sizeof(uint16_t))
SegmentationLabelInventory
scanDenseLabels(const T* const values, const ComponentBufferView& view, const glm::uvec3 dims)
{
  constexpr std::size_t labelCapacity = static_cast<std::size_t>(std::numeric_limits<T>::max()) + 1u;
  std::vector<SegmentationLabelBounds> bounds(labelCapacity);
  std::vector<bool> present(labelCapacity, false);
  const std::size_t width = dims.x;
  const std::size_t sliceSize = width * dims.y;
  for (uint32_t z = 0; z < dims.z; ++z) {
    for (uint32_t y = 0; y < dims.y; ++y) {
      for (uint32_t x = 0; x < dims.x; ++x) {
        const std::size_t index = static_cast<std::size_t>(z) * sliceSize + static_cast<std::size_t>(y) * width + x;
        const std::size_t label = values[index * view.pixelStride + view.componentOffset];
        const glm::uvec3 voxel{x, y, z};
        if (!present[label]) {
          present[label] = true;
          bounds[label] = SegmentationLabelBounds{.minVoxel = voxel, .maxVoxel = voxel};
        }
        else {
          bounds[label].minVoxel = glm::min(bounds[label].minVoxel, voxel);
          bounds[label].maxVoxel = glm::max(bounds[label].maxVoxel, voxel);
        }
        const auto recordSharedBoundary = [&bounds, label](const std::size_t neighbor) {
          if (label == 0u || neighbor == 0u || label == neighbor) {
            return;
          }
          bounds[label].hasSharedBoundary = true;
          bounds[neighbor].hasSharedBoundary = true;
        };
        if (x > 0u) {
          recordSharedBoundary(values[(index - 1u) * view.pixelStride + view.componentOffset]);
        }
        if (y > 0u) {
          recordSharedBoundary(values[(index - width) * view.pixelStride + view.componentOffset]);
        }
        if (z > 0u) {
          recordSharedBoundary(values[(index - sliceSize) * view.pixelStride + view.componentOffset]);
        }
      }
    }
  }

  SegmentationLabelInventory labels;
  labels.reserve(static_cast<std::size_t>(std::ranges::count(present, true)));
  for (std::size_t label = 0; label < labelCapacity; ++label) {
    if (present[label]) {
      labels.emplace(static_cast<int64_t>(label), bounds[label]);
    }
  }
  return labels;
}

template<typename T>
void fillBinaryLabelMask(
  const T* const values,
  const ComponentBufferView& view,
  const glm::uvec3 sourceDimensions,
  const glm::uvec3 sourceMin,
  const glm::uvec3 occupiedSize,
  const int64_t labelValue,
  ScalarGrid3D& grid)
{
  const std::size_t sourceWidth = sourceDimensions.x;
  const std::size_t sourceSliceSize = sourceWidth * sourceDimensions.y;
  for (uint32_t z = 0; z < occupiedSize.z; ++z) {
    for (uint32_t y = 0; y < occupiedSize.y; ++y) {
      for (uint32_t x = 0; x < occupiedSize.x; ++x) {
        const glm::uvec3 sourceVoxel = sourceMin + glm::uvec3{x, y, z};
        const std::size_t sourceIndex = static_cast<std::size_t>(sourceVoxel.z) * sourceSliceSize +
                                        static_cast<std::size_t>(sourceVoxel.y) * sourceWidth + sourceVoxel.x;
        const int64_t value = static_cast<int64_t>(values[sourceIndex * view.pixelStride + view.componentOffset]);
        const glm::uvec3 croppedVoxel = glm::uvec3{x, y, z} + glm::uvec3{1u};
        grid.values[scalarGridValueIndex(grid.dimensions, croppedVoxel.x, croppedVoxel.y, croppedVoxel.z)] =
          value == labelValue ? 1.0f : 0.0f;
      }
    }
  }
}

std::optional<std::size_t> voxelCount(const glm::uvec3 dimensions)
{
  constexpr std::size_t maximum = std::numeric_limits<std::size_t>::max();
  const std::size_t x = dimensions.x;
  const std::size_t y = dimensions.y;
  const std::size_t z = dimensions.z;
  if (x != 0u && y > maximum / x) {
    return std::nullopt;
  }
  const std::size_t xy = x * y;
  if (xy != 0u && z > maximum / xy) {
    return std::nullopt;
  }
  return xy * z;
}

} // namespace

std::optional<SegmentationLabelInventory>
segmentationLabelInventory(const Image& image, const uint32_t component, const uint32_t timePoint)
{
  if (!image.hasPixelData() || component >= image.header().numComponentsPerPixel()) {
    return std::nullopt;
  }

  const glm::uvec3 dimensions = image.header().pixelDimensions();
  if (glm::any(glm::greaterThan(dimensions, glm::uvec3{std::numeric_limits<int>::max()}))) {
    return std::nullopt;
  }
  const std::optional<ComponentBufferView> buffer = componentBufferView(image, component, timePoint);
  if (!buffer) {
    return std::nullopt;
  }
  switch (image.header().memoryComponentType()) {
    case ComponentType::Int8:
      return scanSparseLabels(static_cast<const int8_t*>(buffer->values), *buffer, dimensions);
    case ComponentType::UInt8:
      return scanDenseLabels(static_cast<const uint8_t*>(buffer->values), *buffer, dimensions);
    case ComponentType::Int16:
      return scanSparseLabels(static_cast<const int16_t*>(buffer->values), *buffer, dimensions);
    case ComponentType::UInt16:
      return scanDenseLabels(static_cast<const uint16_t*>(buffer->values), *buffer, dimensions);
    case ComponentType::Int32:
      return scanSparseLabels(static_cast<const int32_t*>(buffer->values), *buffer, dimensions);
    case ComponentType::UInt32:
      return scanSparseLabels(static_cast<const uint32_t*>(buffer->values), *buffer, dimensions);
    default:
      break;
  }

  SegmentationLabelInventory labels;
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
        const auto recordSharedBoundary =
          [&image, component, timePoint, &labels, value](const int x, const int y, const int z) {
            const std::optional<int64_t> neighbor = image.value<int64_t>(component, x, y, z, timePoint);
            if (!neighbor || *value == 0 || *neighbor == 0 || *value == *neighbor) {
              return;
            }
            labels.at(*value).hasSharedBoundary = true;
            labels.at(*neighbor).hasSharedBoundary = true;
          };
        if (i > 0u) {
          recordSharedBoundary(static_cast<int>(i - 1u), static_cast<int>(j), static_cast<int>(k));
        }
        if (j > 0u) {
          recordSharedBoundary(static_cast<int>(i), static_cast<int>(j - 1u), static_cast<int>(k));
        }
        if (k > 0u) {
          recordSharedBoundary(static_cast<int>(i), static_cast<int>(j), static_cast<int>(k - 1u));
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
  const std::optional<std::size_t> numVoxels = voxelCount(grid.dimensions);
  if (!numVoxels) {
    return std::nullopt;
  }
  grid.values.assign(*numVoxels, 0.0f);
  grid.grid_T_voxelIndex = (MeshCoordinateSpace::World == coordinateSpace ? image.transformations().worldDef_T_pixel()
                                                                          : image.transformations().subject_T_pixel()) *
                           glm::translate(glm::mat4{1.0f}, glm::vec3{bounds.minVoxel} - glm::vec3{1.0f});

  const std::optional<ComponentBufferView> buffer = componentBufferView(image, component, timePoint);
  if (!buffer) {
    return std::nullopt;
  }
  switch (image.header().memoryComponentType()) {
    case ComponentType::Int8:
      fillBinaryLabelMask(
        static_cast<const int8_t*>(buffer->values),
        *buffer,
        imageDimensions,
        bounds.minVoxel,
        occupiedSize,
        labelValue,
        grid);
      return grid;
    case ComponentType::UInt8:
      fillBinaryLabelMask(
        static_cast<const uint8_t*>(buffer->values),
        *buffer,
        imageDimensions,
        bounds.minVoxel,
        occupiedSize,
        labelValue,
        grid);
      return grid;
    case ComponentType::Int16:
      fillBinaryLabelMask(
        static_cast<const int16_t*>(buffer->values),
        *buffer,
        imageDimensions,
        bounds.minVoxel,
        occupiedSize,
        labelValue,
        grid);
      return grid;
    case ComponentType::UInt16:
      fillBinaryLabelMask(
        static_cast<const uint16_t*>(buffer->values),
        *buffer,
        imageDimensions,
        bounds.minVoxel,
        occupiedSize,
        labelValue,
        grid);
      return grid;
    case ComponentType::Int32:
      fillBinaryLabelMask(
        static_cast<const int32_t*>(buffer->values),
        *buffer,
        imageDimensions,
        bounds.minVoxel,
        occupiedSize,
        labelValue,
        grid);
      return grid;
    case ComponentType::UInt32:
      fillBinaryLabelMask(
        static_cast<const uint32_t*>(buffer->values),
        *buffer,
        imageDimensions,
        bounds.minVoxel,
        occupiedSize,
        labelValue,
        grid);
      return grid;
    default:
      break;
  }

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

} // namespace rendering::mesh
