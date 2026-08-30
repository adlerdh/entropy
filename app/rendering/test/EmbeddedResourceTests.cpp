#include <catch2/catch_test_macros.hpp>

#include <cmrc/cmrc.hpp>

CMRC_DECLARE(colormaps);
CMRC_DECLARE(fonts);

TEST_CASE("embedded startup resources retain their application paths")
{
  const auto fonts = cmrc::fonts::get_filesystem();
  CHECK(fonts.is_file("res/fonts/Roboto/Roboto-Light.ttf"));
  CHECK(fonts.is_file("res/fonts/Inter/Inter-Regular.ttf"));
  CHECK(fonts.is_file("res/fonts/Cousine/Cousine-Regular.ttf"));

  const auto colormaps = cmrc::colormaps::get_filesystem();
  CHECK(colormaps.is_directory("res/colormaps/matplotlib"));
  CHECK(colormaps.is_file("res/colormaps/matplotlib/viridis.csv"));
}
