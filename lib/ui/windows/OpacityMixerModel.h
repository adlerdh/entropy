#pragma once

#include <cstddef>

namespace entropy::ui::opacity_mixer
{

/**
 * @brief Compute the opacity assigned to one image by the comparison blender.
 * @param imageIndex Index of the image whose opacity is being computed.
 * @param mix Continuous blend position across adjacent image indices.
 * @return Opacity in [0, 1] for the image at imageIndex.
 */
double blendedOpacity(std::size_t imageIndex, double mix);

} // namespace entropy::ui::opacity_mixer
