#include "common/ColorMapDefaults.h"

#include <catch2/catch_test_macros.hpp>
#include <cmrc/cmrc.hpp>

#include <algorithm>
#include <cstddef>
#include <functional>
#include <ranges>
#include <string>
#include <utility>
#include <vector>

CMRC_DECLARE(colormaps);
CMRC_DECLARE(fonts);

TEST_CASE("embedded fonts match the supported runtime resources")
{
  const auto fonts = cmrc::fonts::get_filesystem();
  std::vector<std::string> bundledFontPaths;
  for (const auto& family : fonts.iterate_directory("res/fonts")) {
    REQUIRE(family.is_directory());
    const std::string directory = std::string{"res/fonts/"} + family.filename();
    for (const auto& font : fonts.iterate_directory(directory)) {
      REQUIRE(font.is_file());
      bundledFontPaths.push_back(directory + "/" + font.filename());
    }
  }
  std::sort(bundledFontPaths.begin(), bundledFontPaths.end());

  const std::vector<std::string> expectedFontPaths{
    "res/fonts/Cousine/Cousine-Regular.ttf",
    "res/fonts/ForkAwesome/forkawesome-webfont.ttf",
    "res/fonts/IBMPlexSans/IBMPlexSans-Regular.ttf",
    "res/fonts/Inter/Inter-Bold.ttf",
    "res/fonts/Inter/Inter-Regular.ttf",
    "res/fonts/NotoSans/NotoSans-Regular.ttf",
    "res/fonts/Roboto/Roboto-Light.ttf",
    "res/fonts/Roboto/Roboto-Regular.ttf",
    "res/fonts/SourceSans3/SourceSans3-Regular.ttf",
    "res/fonts/SpaceGrotesk/SpaceGrotesk-Light.ttf"};
  CHECK(bundledFontPaths == expectedFontPaths);
}

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
