#include "layout/LayoutFileSerialization.h"

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
      {.m_type = "threeUp"},
      {.m_type = "oneUp", .m_view = "axial", .m_imageIndices = {0}},
      {.m_type = "grid", .m_view = "axial", .m_images = "all"},
      {.m_type = "lightbox", .m_view = "coronal", .m_imageIndices = {0, 1, 2, 3}}}};

  REQUIRE(layout::save(file, fileName));

  {
    std::ifstream in(fileName);
    REQUIRE(in);
    const std::string json((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    REQUIRE(json.find("\"bounds\"") == std::string::npos);
    REQUIRE(json.find("\"views\"") == std::string::npos);
    REQUIRE(json.find("\"type\": \"threeUp\"") != std::string::npos);
    REQUIRE(json.find("\"type\": \"oneUp\"") != std::string::npos);
    REQUIRE(json.find("\"type\": \"grid\"") != std::string::npos);
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
    {"type": "oneUp", "view": "axial", "images": [0]},
    {"type": "grid", "view": "coronal", "images": "all"},
    {"type": "lightbox", "view": "sagittal", "images": [2]}
  ]
})";
  }

  layout::LayoutFile file;
  REQUIRE(layout::open(file, fileName));
  REQUIRE(file.m_currentLayoutIndex == 1);
  REQUIRE(file.m_layouts.size() == 4);
  REQUIRE(file.m_layouts.at(0).m_type == "fourUp");
  REQUIRE(file.m_layouts.at(1).m_type == "oneUp");
  REQUIRE(file.m_layouts.at(1).m_view == "axial");
  REQUIRE(file.m_layouts.at(1).m_imageIndices == std::vector<std::size_t>{0});
  REQUIRE(file.m_layouts.at(2).m_type == "grid");
  REQUIRE(file.m_layouts.at(2).m_view == "coronal");
  REQUIRE(file.m_layouts.at(2).m_images == "all");
  REQUIRE(file.m_layouts.at(3).m_type == "lightbox");
  REQUIRE(file.m_layouts.at(3).m_view == "sagittal");
  REQUIRE(file.m_layouts.at(3).m_imageIndices == std::vector<std::size_t>{2});

  std::filesystem::remove(fileName);
}
