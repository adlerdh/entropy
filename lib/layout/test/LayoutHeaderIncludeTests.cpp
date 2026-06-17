#include "layout/ImageIndexMapping.h"
#include "layout/LayoutFileSerialization.h"
#include "layout/LayoutKindInfo.h"
#include "layout/LayoutPreset.h"
#include "layout/LayoutPresetInfo.h"
#include "layout/LayoutSpec.h"
#include "layout/LayoutSpecJson.h"
#include "layout/SyncGroupIndexMap.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("layout public headers are self-contained")
{
  layout::LayoutPreset preset;
  layout::LayoutFile file;
  layout::ImageSelectionSpec selection;
  layout::ViewSpec view;
  layout::LayoutSpec spec;
  layout::SyncGroupIndexMap syncGroups;

  CHECK(preset.m_type.empty());
  CHECK(file.m_layouts.empty());
  CHECK(selection.m_renderedImageIndices.empty());
  CHECK(view.m_defaultRenderAllImages);
  CHECK(spec.m_views.empty());
  CHECK_FALSE(layout::imageUidForIndex({}, std::nullopt));
  CHECK(layout::layoutDisplayName(LayoutKind::Custom, false) == "Custom");
  CHECK(layout::layoutPresetViewName(ViewType::Axial) == "axial");
  CHECK(layout::lightboxLayoutKindForViewType(ViewType::Axial) == LayoutKind::AxialLightbox);
  CHECK_FALSE(syncGroups.groupUidForIndex(std::nullopt));
}
