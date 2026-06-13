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
 * @brief Render the mode toolbar and its active-image selector.
 */
void renderModeToolbar(
  AppData& appData,
  const std::function<MouseMode(void)>& getMouseMode,
  const std::function<void(MouseMode)>& setMouseMode,
  const std::function<void(void)>& readjustViewport,
  const AllViewsRecenterType& recenterAllViews,
  const std::function<bool(void)>& getOverlayVisibility,
  const std::function<void(bool)>& setOverlayVisibility,
  const std::function<void(int step)>& cycleViews,
  std::size_t numImages,
  const std::function<std::pair<std::string, std::string>(std::size_t index)>& getImageDisplayAndFileName,
  const std::function<std::size_t(void)>& getActiveImageIndex,
  const std::function<void(std::size_t)>& setActiveImageIndex);
