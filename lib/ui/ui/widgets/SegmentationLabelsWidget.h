#pragma once

#include <cstddef>
#include <functional>

class ParcellationLabelTable;

/**
 * @brief Render the editable label table child window for one segmentation.
 */
void renderSegLabelsChildWindow(
  std::size_t tableIndex,
  ParcellationLabelTable* labelTable,
  std::size_t selectedLabel,
  const std::function<void(std::size_t labelIndex)>& selectLabel,
  const std::function<void(std::size_t tableIndex)>& updateLabelColorTableTexture,
  const std::function<void(std::size_t labelIndex)>& moveCrosshairsToSegLabelCentroid);
