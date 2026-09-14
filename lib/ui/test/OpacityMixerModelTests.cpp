#include "ui/windows/OpacityMixerModel.h"

#include <catch2/catch_test_macros.hpp>

namespace opacity_mixer = ui::opacity_mixer;

TEST_CASE("opacity mixer assigns all opacity to an exact image index", "[ui][opacity]")
{
  CHECK(opacity_mixer::blendedOpacity(0, 1.0) == 0.0);
  CHECK(opacity_mixer::blendedOpacity(1, 1.0) == 1.0);
  CHECK(opacity_mixer::blendedOpacity(2, 1.0) == 0.0);
}

TEST_CASE("opacity mixer blends between adjacent image indices", "[ui][opacity]")
{
  CHECK(opacity_mixer::blendedOpacity(0, 0.25) == 0.75);
  CHECK(opacity_mixer::blendedOpacity(1, 0.25) == 0.25);
  CHECK(opacity_mixer::blendedOpacity(2, 0.25) == 0.0);

  CHECK(opacity_mixer::blendedOpacity(1, 1.75) == 0.25);
  CHECK(opacity_mixer::blendedOpacity(2, 1.75) == 0.75);
  CHECK(opacity_mixer::blendedOpacity(3, 1.75) == 0.0);
}
