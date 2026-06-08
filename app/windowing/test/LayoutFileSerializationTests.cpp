#include "windowing/LayoutFileSerialization.h"

#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <vector>

namespace
{

std::filesystem::path tempLayoutFile(const char* name)
{
  return std::filesystem::temp_directory_path() / name;
}

} // namespace

TEST_CASE("Layout files save compact layout presets", "[layout]")
{
  const auto fileName = tempLayoutFile("entropy-layout-presets-save.json");

  const layout::LayoutFile file{
    .m_currentLayoutIndex = 2,
    .m_layouts = {
      {.m_type = "fourUp"},
      {.m_type = "multiImageGrid", .m_view = "axial", .m_images = "all"},
      {.m_type = "lightbox", .m_view = "coronal", .m_imageIndices = {0, 1, 2, 3}}}};

  REQUIRE(layout::save(file, fileName));

  {
    std::ifstream in(fileName);
    REQUIRE(in);
    const std::string json((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    REQUIRE(json.find("\"bounds\"") == std::string::npos);
    REQUIRE(json.find("\"views\"") == std::string::npos);
    REQUIRE(json.find("\"type\": \"lightbox\"") != std::string::npos);
    REQUIRE(json.find("\"image\":") == std::string::npos);
    REQUIRE(json.find("\"images\": [0, 1, 2, 3]") != std::string::npos);
  }

  std::filesystem::remove(fileName);
}

TEST_CASE("Layout files load compact layout presets", "[layout]")
{
  const auto fileName = tempLayoutFile("entropy-layout-presets-open.json");
  {
    std::ofstream out(fileName);
    REQUIRE(out);
    out << R"({
  "format": "EntropyLayouts",
  "version": 1,
  "currentLayout": 1,
  "layouts": [
    {"type": "fourUp"},
    {"type": "single", "view": "axial", "images": [0]},
    {"type": "lightbox", "view": "sagittal", "images": [2]}
  ]
})";
  }

  layout::LayoutFile file;
  REQUIRE(layout::open(file, fileName));
  REQUIRE(file.m_currentLayoutIndex == 1);
  REQUIRE(file.m_layouts.size() == 3);
  REQUIRE(file.m_layouts.at(0).m_type == "fourUp");
  REQUIRE(file.m_layouts.at(1).m_type == "single");
  REQUIRE(file.m_layouts.at(1).m_view == "axial");
  REQUIRE(file.m_layouts.at(1).m_imageIndices == std::vector<std::size_t>{0});
  REQUIRE(file.m_layouts.at(2).m_type == "lightbox");
  REQUIRE(file.m_layouts.at(2).m_view == "sagittal");
  REQUIRE(file.m_layouts.at(2).m_imageIndices == std::vector<std::size_t>{2});

  std::filesystem::remove(fileName);
}

TEST_CASE("Layout files still load legacy singular image fields", "[layout]")
{
  const auto fileName = tempLayoutFile("entropy-layout-presets-legacy-image.json");
  {
    std::ofstream out(fileName);
    REQUIRE(out);
    out << R"([
  {"type": "single", "view": "axial", "image": 0},
  {"type": "lightbox", "view": "coronal", "image": "reference"}
])";
  }

  layout::LayoutFile file;
  REQUIRE(layout::open(file, fileName));
  REQUIRE(!file.m_currentLayoutIndex);
  REQUIRE(file.m_layouts.size() == 2);
  REQUIRE(file.m_layouts.at(0).m_imageIndices == std::vector<std::size_t>{0});
  REQUIRE(file.m_layouts.at(1).m_imageIndices == std::vector<std::size_t>{0});

  std::filesystem::remove(fileName);
}
