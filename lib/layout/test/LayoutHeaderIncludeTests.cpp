#include "layout/LayoutFileSerialization.h"
#include "layout/LayoutPreset.h"
#include "layout/LayoutSpec.h"
#include "layout/LayoutSpecJson.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("layout public headers are self-contained")
{
  layout::LayoutPreset preset;
  layout::LayoutFile file;
  layout::ImageSelectionSpec selection;
  layout::ViewSpec view;
  layout::LayoutSpec spec;

  CHECK(preset.m_type.empty());
  CHECK(file.m_layouts.empty());
  CHECK(selection.m_renderedImageIndices.empty());
  CHECK(view.m_defaultRenderAllImages);
  CHECK(spec.m_views.empty());
}
