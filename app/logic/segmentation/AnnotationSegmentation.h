#pragma once

#include "common/Types.h"

#include <glm/fwd.hpp>

#include <functional>

class Annotation;
class Image;

/**
 * @brief Paint a segmentation label into voxels covered by a closed annotation polygon
 * @param seg Segmentation image to modify
 * @param annot Annotation whose polygon defines the painted region
 * @param labelToPaint Label value written into matching voxels
 * @param labelToReplace Label value eligible for replacement
 * @param brushReplacesBgWithFg True when foreground painting may replace background voxels
 * @param updateSegTexture Callback used to update the affected segmentation texture region
 * @throw Propagates exceptions from image access, polygon evaluation, or the texture update callback
 */
void fillSegmentationWithPolygon(
  Image& seg,
  const Annotation* annot,

  int64_t labelToPaint,
  int64_t labelToReplace,
  bool brushReplacesBgWithFg,

  const std::function<void(
    const ComponentType& memoryComponentType,
    const glm::uvec3& offset,
    const glm::uvec3& size,
    const int64_t* data)>& updateSegTexture);
