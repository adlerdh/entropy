#pragma once

#include "common/Types.h"

#include <glm/fwd.hpp>

#include <functional>

class Annotation;
class Image;

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
