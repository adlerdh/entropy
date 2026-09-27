#pragma once

#include "image/Image.h"

#include <glm/gtc/matrix_transform.hpp>

#include <array>
#include <functional>
#include <vector>

namespace entropy::test
{
struct Grid
{
  glm::uvec3 size{9, 11, 7};
  glm::dvec3 spacing{0.7, 1.3, 2.1};
  glm::dvec3 origin{13, -9, 4};
  glm::dmat3 direction{glm::rotate(glm::dmat4{1}, 0.37, glm::normalize(glm::dvec3{1, 2, 3}))};
  glm::dvec3 subject(glm::uvec3 voxel) const
  {
    return origin + direction * (spacing * glm::dvec3{voxel});
  }
};

inline Image
analyticImage(const Grid& grid, const std::function<glm::vec3(glm::dvec3, glm::uvec3)>& sample, bool vector = false)
{
  ImageIoInfo info;
  info.m_componentInfo.m_componentType = ComponentType::Float32;
  info.m_componentInfo.m_componentSizeInBytes = sizeof(float);
  info.m_pixelInfo.m_pixelType = vector ? PixelType::Vector : PixelType::Scalar;
  info.m_pixelInfo.m_numComponents = vector ? 3 : 1;
  info.m_pixelInfo.m_pixelStrideInBytes = info.m_pixelInfo.m_numComponents * sizeof(float);
  const std::size_t count = static_cast<std::size_t>(grid.size.x) * grid.size.y * grid.size.z;
  info.m_sizeInfo.m_imageSizeInPixels = count;
  info.m_sizeInfo.m_imageSizeInComponents = count * info.m_pixelInfo.m_numComponents;
  info.m_sizeInfo.m_imageSizeInBytes = info.m_sizeInfo.m_imageSizeInComponents * sizeof(float);
  info.m_spaceInfo.m_numDimensions = 3;
  info.m_spaceInfo.m_dimensions = {grid.size.x, grid.size.y, grid.size.z};
  info.m_spaceInfo.m_spacing = {grid.spacing.x, grid.spacing.y, grid.spacing.z};
  info.m_spaceInfo.m_origin = {grid.origin.x, grid.origin.y, grid.origin.z};
  for (int c = 0; c < 3; ++c) {
    info.m_spaceInfo.m_directions.push_back({grid.direction[c].x, grid.direction[c].y, grid.direction[c].z});
  }

  std::array<std::vector<float>, 3> values;
  for (auto& component : values)
    component.resize(count);

  std::size_t index = 0;
  for (unsigned z = 0; z < grid.size.z; ++z)
    for (unsigned y = 0; y < grid.size.y; ++y)
      for (unsigned x = 0; x < grid.size.x; ++x, ++index) {
        const auto value = sample(grid.subject({x, y, z}), {x, y, z});
        for (int c = 0; c < 3; ++c)
          values[c][index] = value[c];
      }

  std::vector<const void*> buffers{values[0].data()};
  if (vector) {
    buffers.push_back(values[1].data());
    buffers.push_back(values[2].data());
  }

  auto image = Image::fromCopiedData(
    ImageHeader{info, info, false},
    "analytic",
    Image::ImageRepresentation::Image,
    Image::MultiComponentBufferType::SeparateImages,
    buffers);
  image.header().setExistsOnDisk(false);
  return image;
}

inline Image scalarRamp(const Grid& grid = {})
{
  return analyticImage(grid, [](auto p, auto) { return glm::vec3{static_cast<float>(2 * p.x - 3 * p.y + p.z)}; });
}

inline Image translationField(const Grid& grid, glm::vec3 translation)
{
  return analyticImage(grid, [translation](auto, auto) { return translation; }, true);
}

inline Image bendingField(const Grid& grid)
{
  return analyticImage(grid, [](auto p, auto) { return glm::vec3{static_cast<float>(0.001 * p.y * p.y), 0, 0}; }, true);
}

inline glm::mat4 shear()
{
  glm::mat4 result{1};
  result[1][0] = 0.25f;
  result[2][1] = -0.125f;
  result[3] = {3, -2, 1, 1};
  return result;
}

inline Image labels(const Grid& grid = {})
{
  ImageIoInfo info;
  info.m_componentInfo.m_componentType = ComponentType::UInt16;
  info.m_componentInfo.m_componentSizeInBytes = sizeof(uint16_t);
  info.m_pixelInfo.m_pixelType = PixelType::Scalar;
  info.m_pixelInfo.m_numComponents = 1;
  info.m_pixelInfo.m_pixelStrideInBytes = sizeof(uint16_t);

  const std::size_t count = static_cast<std::size_t>(grid.size.x) * grid.size.y * grid.size.z;
  info.m_sizeInfo.m_imageSizeInPixels = count;
  info.m_sizeInfo.m_imageSizeInComponents = count;
  info.m_sizeInfo.m_imageSizeInBytes = count * sizeof(uint16_t);
  info.m_spaceInfo.m_numDimensions = 3;
  info.m_spaceInfo.m_dimensions = {grid.size.x, grid.size.y, grid.size.z};
  info.m_spaceInfo.m_spacing = {grid.spacing.x, grid.spacing.y, grid.spacing.z};
  info.m_spaceInfo.m_origin = {grid.origin.x, grid.origin.y, grid.origin.z};

  for (int c = 0; c < 3; ++c)
    info.m_spaceInfo.m_directions.push_back({grid.direction[c].x, grid.direction[c].y, grid.direction[c].z});

  std::vector<uint16_t> pixels(count, 0);
  auto image = Image::fromCopiedData(
    ImageHeader{info, info, false},
    "labels",
    Image::ImageRepresentation::Segmentation,
    Image::MultiComponentBufferType::SeparateImages,
    {pixels.data()});
  image.header().setExistsOnDisk(false);
  return image;
}
} // namespace entropy::test
