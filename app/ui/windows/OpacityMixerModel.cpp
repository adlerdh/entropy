#include "ui/windows/OpacityMixerModel.h"

#include <cmath>

namespace entropy::ui::opacity_mixer
{

double blendedOpacity(std::size_t imageIndex, double mix)
{
  const double frac = mix - std::floor(mix);
  const std::size_t imageIndexLo = static_cast<std::size_t>(std::floor(mix));
  const std::size_t imageIndexHi = static_cast<std::size_t>(std::ceil(mix));

  if (imageIndex < imageIndexLo || imageIndexHi < imageIndex) {
    return 0.0;
  }

  if (imageIndexLo == imageIndex) {
    return 1.0 - frac;
  }

  if (imageIndexHi == imageIndex) {
    return frac;
  }

  return 0.0;
}

} // namespace entropy::ui::opacity_mixer
