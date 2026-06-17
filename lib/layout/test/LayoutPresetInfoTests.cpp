#include "layout/LayoutPresetInfo.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("layout preset view names are stable", "[layout][preset]")
{
  CHECK(layout::layoutPresetViewName(ViewType::Axial) == "axial");
  CHECK(layout::layoutPresetViewName(ViewType::Coronal) == "coronal");
  CHECK(layout::layoutPresetViewName(ViewType::Sagittal) == "sagittal");
  CHECK(layout::layoutPresetViewName(ViewType::Oblique) == "oblique");
  CHECK(layout::layoutPresetViewName(ViewType::ThreeD) == "ThreeD");
  CHECK(layout::layoutPresetViewName(ViewType::NumElements) == "axial");
}

TEST_CASE("layout preset view type accepts saved aliases", "[layout][preset]")
{
  CHECK(layout::layoutPresetViewType({}) == ViewType::Axial);
  CHECK(layout::layoutPresetViewType("Axial") == ViewType::Axial);
  CHECK(layout::layoutPresetViewType("axial") == ViewType::Axial);
  CHECK(layout::layoutPresetViewType("Coronal") == ViewType::Coronal);
  CHECK(layout::layoutPresetViewType("coronal") == ViewType::Coronal);
  CHECK(layout::layoutPresetViewType("Sagittal") == ViewType::Sagittal);
  CHECK(layout::layoutPresetViewType("sagittal") == ViewType::Sagittal);
  CHECK(layout::layoutPresetViewType("Oblique") == ViewType::Oblique);
  CHECK(layout::layoutPresetViewType("oblique") == ViewType::Oblique);
  CHECK(layout::layoutPresetViewType("ThreeD") == ViewType::ThreeD);
  CHECK(layout::layoutPresetViewType("3D") == ViewType::ThreeD);
  CHECK(layout::layoutPresetViewType("threeD") == ViewType::ThreeD);
  CHECK(layout::layoutPresetViewType("three-d") == ViewType::ThreeD);
}

TEST_CASE("layout preset view type rejects unsupported names", "[layout][preset]")
{
  CHECK_FALSE(layout::layoutPresetViewType("transverse"));
}

TEST_CASE("preset image indices default to the first image", "[layout][preset]")
{
  layout::LayoutPreset preset;

  CHECK(layout::presetImageIndicesOrDefault(preset) == std::vector<std::size_t>{0});

  preset.m_imageIndices = {2, 4};

  CHECK(layout::presetImageIndicesOrDefault(preset) == std::vector<std::size_t>{2, 4});
}
