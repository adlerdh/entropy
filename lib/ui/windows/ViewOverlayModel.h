#pragma once

#include "viewer/ViewModes.h"

#include <cstddef>
#include <string>
#include <vector>

namespace ui::view_overlay
{

/**
 * @brief Metadata for one image in a view overlay image-selection control.
 */
struct ImageChoice
{
  std::string displayName; //!< User-visible image name
  bool visible = true;     //!< Whether the image is globally visible
  bool active = false;     //!< Whether the image is the active image
  bool reference = false;  //!< Whether the image is the reference image
  bool selected = false;   //!< Whether the image is selected for the current view operation
};

/**
 * @brief Return true when a render mode uses image-layer selection instead of metric selection.
 * @param renderMode View render mode.
 * @return True for image and volume rendering modes.
 */
bool usesVisibleImageSelection(ViewRenderMode renderMode);

/**
 * @brief Return true when a render mode should show metric-image selection.
 * @param renderMode View render mode.
 * @return True for comparison/metric render modes.
 */
bool usesMetricImageSelection(ViewRenderMode renderMode);

/**
 * @brief Return true when the image-selection overlay should show a disabled icon.
 * @param renderMode View render mode.
 * @return True when the render mode is disabled.
 */
bool usesDisabledVisibilityIcon(ViewRenderMode renderMode);

/**
 * @brief Build the display label for an image selection menu item.
 * @param choice Image metadata and state.
 * @return Display label with hidden/active qualifiers appended.
 */
std::string imageChoiceLabel(const ImageChoice& choice);

/**
 * @brief Join the names of selected and visible images for the view overlay label.
 * @param choices Image choices in display order.
 * @return Comma-separated selected image labels, or an empty string when none are selected.
 */
std::string selectedVisibleImageNames(const std::vector<ImageChoice>& choices);

} // namespace ui::view_overlay
