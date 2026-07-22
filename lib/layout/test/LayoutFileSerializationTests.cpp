#include "layout/LayoutFileSerialization.h"

#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <set>
#include <vector>

namespace
{

std::filesystem::path tempLayoutFile(const char* name)
{
  return std::filesystem::temp_directory_path() / name;
}

} // namespace

TEST_CASE("Layout files save layout snapshots", "[layout]")
{
  const auto fileName = tempLayoutFile("entropy-layout-snapshots-save.json");

  layout::LayoutSpec layout;
  layout.m_kind = 3;
  layout.m_displayName = "Review";
  layout.m_viewType = 1;
  layout.m_imageSelection.m_renderedImageIndices = {0, 1};
  layout.m_views.push_back(layout::ViewSpec{});

  const layout::LayoutFile file{.m_currentLayoutIndex = 2, .m_layouts = {layout}};

  REQUIRE(layout::save(file, fileName));

  {
    std::ifstream in(fileName);
    REQUIRE(in);
    const std::string json((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    REQUIRE(json.find("\"format\": \"EntropyLayouts\"") != std::string::npos);
    REQUIRE(json.find("\"version\": {") != std::string::npos);
    REQUIRE(json.find("\"currentLayout\": 2") != std::string::npos);
    REQUIRE(json.find("\"kind\": \"oneUp\"") != std::string::npos);
    REQUIRE(json.find("\"displayName\": \"Review\"") != std::string::npos);
    REQUIRE(json.find("\"views\": [") != std::string::npos);
    REQUIRE(json.find("\"images\": {") != std::string::npos);
    REQUIRE(json.find("\"type\":") == std::string::npos);
  }

  std::filesystem::remove(fileName);
}

TEST_CASE("Layout files load layout snapshots", "[layout]")
{
  const auto fileName = tempLayoutFile("entropy-layout-snapshots-open.json");
  {
    std::ofstream out(fileName);
    REQUIRE(out);
    out << R"({
  "format": "EntropyLayouts",
  "version": {"major": 1, "minor": 0},
  "currentLayout": 1,
  "layouts": [
    {
      "kind": "oneUp",
      "displayName": "Review",
      "isLightbox": false,
      "viewType": "axial",
      "renderMode": "image",
      "intensityProjectionMode": "none",
      "defaultImages": {"preferred": [0], "renderAll": false},
      "images": {"rendered": [0]},
      "views": [
        {
          "viewport": {"left": -1.0, "bottom": -1.0, "width": 2.0, "height": 2.0},
          "viewType": "axial",
          "renderMode": "image",
          "intensityProjectionMode": "none"
        }
      ]
    },
    {
      "kind": "multiImageGrid",
      "isLightbox": false,
      "viewType": "coronal",
      "renderMode": "overlay",
      "intensityProjectionMode": "maximum",
      "views": [
        {
          "viewport": {"left": -1.0, "bottom": -1.0, "width": 2.0, "height": 2.0},
          "viewType": "coronal",
          "renderMode": "overlay",
          "intensityProjectionMode": "maximum"
        }
      ]
    }
  ]
})";
  }

  layout::LayoutFile file;
  REQUIRE(layout::open(file, fileName));
  REQUIRE(file.m_currentLayoutIndex == 1);
  REQUIRE(file.m_layouts.size() == 2);
  REQUIRE(file.m_layouts.at(0).m_kind == 3);
  REQUIRE(file.m_layouts.at(0).m_displayName == "Review");
  REQUIRE(file.m_layouts.at(0).m_viewType == 0);
  REQUIRE(file.m_layouts.at(0).m_renderMode == 0);
  REQUIRE(file.m_layouts.at(0).m_preferredDefaultRenderedImages == std::set<std::size_t>{0});
  REQUIRE(file.m_layouts.at(0).m_defaultRenderAllImages == false);
  REQUIRE(file.m_layouts.at(0).m_imageSelection.m_renderedImageIndices == std::vector<std::size_t>{0});
  REQUIRE(file.m_layouts.at(0).m_views.size() == 1);
  REQUIRE(file.m_layouts.at(1).m_kind == 4);
  REQUIRE(file.m_layouts.at(1).m_viewType == 1);
  REQUIRE(file.m_layouts.at(1).m_renderMode == 4);

  std::filesystem::remove(fileName);
}
