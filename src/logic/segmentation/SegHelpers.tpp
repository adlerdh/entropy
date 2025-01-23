#ifndef SEGMENTATION_HELPERS_TPP
#define SEGMENTATION_HELPERS_TPP

#include "common/SegmentationTypes.h"

#include <glm/glm.hpp>

#include <optional>

template<typename T>
std::optional<glm::vec3> computePixelCentroid(
  const void* data, const glm::ivec3& dims, const LabelType& label
)
{
  glm::vec3 coordSum{0.0f, 0.0f, 0.0f};
  std::size_t count = 0;

  const T* dataCast = static_cast<const T*>(data);

  for (int k = 0; k < dims.z; ++k)
  {
    for (int j = 0; j < dims.y; ++j)
    {
      for (int i = 0; i < dims.x; ++i)
      {
        if (label == static_cast<LabelType>(dataCast[k * dims.x * dims.y + j * dims.x + i]))
        {
          coordSum += glm::vec3{i, j, k};
          ++count;
        }
      }
    }
  }

  if (0 == count)
  {
    // No voxels found with this segmentation label. Return null so that we don't
    // divide by zero and move crosshairs to an invalid location.
    return std::nullopt;
  }

  return coordSum / static_cast<float>(count);
}

template<typename T>
LabelIndexMaps createLabelIndexMaps(
  const glm::ivec3& dims, const T* buffer, bool ignoreBackgroundZeroLabel
)
{
  LabelIndexMaps labelMaps;

  std::size_t labelIndex = 0;

  for (int z = 0; z < dims.z; ++z)
  {
    for (int y = 0; y < dims.y; ++y)
    {
      for (int x = 0; x < dims.x; ++x)
      {
        const LabelType label = buffer[z * dims.x * dims.y + y * dims.x + x];

        // Ignore the background (0) label
        if (0 < label || (0 == label && !ignoreBackgroundZeroLabel))
        {
          const auto [iter, inserted] = labelMaps.labelToIndex.emplace(label, labelIndex);

          if (inserted)
          {
            labelMaps.indexToLabel.emplace(labelIndex++, label);
          }
        }
      }
    }
  }

  return labelMaps;
}

#endif // SEGMENTATION_HELPERS_TPP
