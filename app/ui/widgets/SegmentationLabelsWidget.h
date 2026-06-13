#pragma once

#include "common/PublicTypes.h"

#include <glm/fwd.hpp>

#include <cstddef>
#include <functional>
#include <string>
#include <utility>

class AppData;
class ImageColorMap;
class ImageTransformations;
class LandmarkGroup;
class ParcellationLabelTable;

/**
 * @brief Render the editable label table child window for one segmentation.
 */
void renderSegLabelsChildWindow(
  std::size_t tableIndex,
  ParcellationLabelTable* labelTable,
  const std::function<void(std::size_t tableIndex)>& updateLabelColorTableTexture,
  const std::function<void(std::size_t labelIndex)>& moveCrosshairsToSegLabelCentroid);
