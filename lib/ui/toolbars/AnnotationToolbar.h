#pragma once

#include "common/PublicTypes.h"
#include "common/SegmentationTypes.h"
#include "common/Types.h"

#include <uuid.h>

#include <cstddef>
#include <functional>
#include <string>
#include <utility>

class AppData;

/**
 * @brief Render the annotation toolbar for the active annotation mode.
 */
void renderAnnotationToolbar(
  AppData& appData,
  const FrameBounds& mindowFrameBounds,
  const std::function<void()>& paintActiveAnnotation);
