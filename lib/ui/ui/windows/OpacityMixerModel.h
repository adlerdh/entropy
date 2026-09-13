#pragma once

#include <cstddef>

namespace ui::opacity_mixer
{

/**
 * @brief Compute the opacity assigned to one image by the comparison blender.
 * @param imageIndex Index of the image whose opacity is being computed.
 * @param mix Continuous blend position across adjacent image indices.
 * @return Opacity in [0, 1] for the image at imageIndex.
 */
double blendedOpacity(std::size_t imageIndex, double mix);

/**
 * @brief Return whether loading images should automatically open the opacity mixer.
 *
 * The mixer opens when the number of loaded images crosses from at most one to
 * more than one. This leaves users free to close it while working with the
 * current set of images, while reopening it when a later load again creates a
 * multi-image comparison.
 */
bool shouldOpenForImageCountTransition(std::size_t previousImageCount, std::size_t currentImageCount);

} // namespace ui::opacity_mixer
