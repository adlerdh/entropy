#include "common/ColorMapDefaults.h"

#include <catch2/catch_test_macros.hpp>
#include <cmrc/cmrc.hpp>

#include <algorithm>
#include <string>
#include <vector>

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

  const auto csvFiles = [&colormaps](const std::string& directory) {
    std::vector<std::string> files;
    for (const auto& item : colormaps.iterate_directory(directory)) {
      if (item.is_file() && item.filename().ends_with(".csv")) {
        files.emplace_back(item.filename());
      }
    }
    std::ranges::sort(files);
    return files;
  };

  const auto matplotlibMaps = csvFiles("res/colormaps/matplotlib/");
  const auto nclMaps = csvFiles("res/colormaps/ncl/");
  const auto peterKovesiMaps = csvFiles("res/colormaps/peter_kovesi/");
  constexpr std::size_t kProgrammaticMaps = 18;
  const std::size_t gouldianOffset =
    static_cast<std::size_t>(std::ranges::find(peterKovesiMaps, "CET-L20.csv") - peterKovesiMaps.begin());
  CHECK(
    kProgrammaticMaps + matplotlibMaps.size() + nclMaps.size() + gouldianOffset ==
    colormap_defaults::kLinear20GouldianIndex);
}
