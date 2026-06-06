#include "windowing/LayoutSpecJson.h"

#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

namespace
{

layout::LayoutSpec makePopulatedLayoutSpec()
{
  layout::LayoutSpec spec;
  spec.m_kind = 5;
  spec.m_isLightbox = true;
  spec.m_viewType = 1;
  spec.m_renderMode = 2;
  spec.m_intensityProjectionMode = 3;
  spec.m_preferredDefaultRenderedImages = {0, 2};
  spec.m_defaultRenderAllImages = false;
  spec.m_imageSelection.m_renderedImageIndices = {2};
  spec.m_imageSelection.m_metricImageIndices = {0, 2};

  layout::ViewSpec first;
  first.m_left = -1.0f;
  first.m_bottom = -1.0f;
  first.m_width = 1.0f;
  first.m_height = 2.0f;
  first.m_viewType = 0;
  first.m_renderMode = 1;
  first.m_intensityProjectionMode = 2;
  first.m_offsetMode = 1;
  first.m_absoluteOffset = 4.5f;
  first.m_relativeOffsetSteps = -2;
  first.m_offsetImageIndex = 2;
  first.m_rotationSyncGroup = 0;
  first.m_translationSyncGroup = 1;
  first.m_zoomSyncGroup = 2;
  first.m_rotationSyncMembershipGroup = 0;
  first.m_translationSyncMembershipGroup = 1;
  first.m_zoomSyncMembershipGroup = 2;
  first.m_preferredDefaultRenderedImages = {1};
  first.m_defaultRenderAllImages = false;
  first.m_imageSelection.m_renderedImageIndices = {1};
  first.m_imageSelection.m_metricImageIndices = {0, 1};

  layout::ViewSpec second;
  second.m_left = 0.0f;
  second.m_bottom = -1.0f;
  second.m_width = 1.0f;
  second.m_height = 2.0f;
  second.m_viewType = 2;
  second.m_renderMode = 3;
  second.m_intensityProjectionMode = 4;
  second.m_offsetMode = 3;
  second.m_relativeOffsetSteps = 3;
  second.m_rotationSyncGroup = 0;
  second.m_translationSyncGroup = 1;
  second.m_zoomSyncMembershipGroup = 2;
  second.m_defaultRenderAllImages = true;
  second.m_imageSelection.m_renderedImageIndices = {0, 1, 2};

  spec.m_views = {first, second};
  return spec;
}

void requireSame(const layout::ImageSelectionSpec& actual, const layout::ImageSelectionSpec& expected)
{
  REQUIRE(actual.m_renderedImageIndices == expected.m_renderedImageIndices);
  REQUIRE(actual.m_metricImageIndices == expected.m_metricImageIndices);
}

void requireSame(const layout::ViewSpec& actual, const layout::ViewSpec& expected)
{
  REQUIRE(actual.m_left == expected.m_left);
  REQUIRE(actual.m_bottom == expected.m_bottom);
  REQUIRE(actual.m_width == expected.m_width);
  REQUIRE(actual.m_height == expected.m_height);
  REQUIRE(actual.m_viewType == expected.m_viewType);
  REQUIRE(actual.m_renderMode == expected.m_renderMode);
  REQUIRE(actual.m_intensityProjectionMode == expected.m_intensityProjectionMode);
  REQUIRE(actual.m_offsetMode == expected.m_offsetMode);
  REQUIRE(actual.m_absoluteOffset == expected.m_absoluteOffset);
  REQUIRE(actual.m_relativeOffsetSteps == expected.m_relativeOffsetSteps);
  REQUIRE(actual.m_offsetImageIndex == expected.m_offsetImageIndex);
  REQUIRE(actual.m_rotationSyncGroup == expected.m_rotationSyncGroup);
  REQUIRE(actual.m_translationSyncGroup == expected.m_translationSyncGroup);
  REQUIRE(actual.m_zoomSyncGroup == expected.m_zoomSyncGroup);
  REQUIRE(actual.m_rotationSyncMembershipGroup == expected.m_rotationSyncMembershipGroup);
  REQUIRE(actual.m_translationSyncMembershipGroup == expected.m_translationSyncMembershipGroup);
  REQUIRE(actual.m_zoomSyncMembershipGroup == expected.m_zoomSyncMembershipGroup);
  REQUIRE(actual.m_preferredDefaultRenderedImages == expected.m_preferredDefaultRenderedImages);
  REQUIRE(actual.m_defaultRenderAllImages == expected.m_defaultRenderAllImages);
  requireSame(actual.m_imageSelection, expected.m_imageSelection);
}

void requireSame(const layout::LayoutSpec& actual, const layout::LayoutSpec& expected)
{
  REQUIRE(actual.m_kind == expected.m_kind);
  REQUIRE(actual.m_isLightbox == expected.m_isLightbox);
  REQUIRE(actual.m_viewType == expected.m_viewType);
  REQUIRE(actual.m_renderMode == expected.m_renderMode);
  REQUIRE(actual.m_intensityProjectionMode == expected.m_intensityProjectionMode);
  REQUIRE(actual.m_preferredDefaultRenderedImages == expected.m_preferredDefaultRenderedImages);
  REQUIRE(actual.m_defaultRenderAllImages == expected.m_defaultRenderAllImages);
  requireSame(actual.m_imageSelection, expected.m_imageSelection);
  REQUIRE(actual.m_views.size() == expected.m_views.size());
  for (std::size_t i = 0; i < actual.m_views.size(); ++i) {
    requireSame(actual.m_views.at(i), expected.m_views.at(i));
  }
}

} // namespace

TEST_CASE("layout specs round-trip through JSON", "[layout][serialization]")
{
  const layout::LayoutSpec original = makePopulatedLayoutSpec();

  const nlohmann::json json = original;
  const layout::LayoutSpec restored = json.get<layout::LayoutSpec>();

  requireSame(restored, original);
}

TEST_CASE("layout spec JSON preserves view order", "[layout][serialization]")
{
  const layout::LayoutSpec original = makePopulatedLayoutSpec();

  const nlohmann::json json = original;
  REQUIRE(json.at("views").size() == 2);
  REQUIRE(json.at("views").at(0).at("viewport").at("left").get<float>() == -1.0f);
  REQUIRE(json.at("views").at(1).at("viewport").at("left").get<float>() == 0.0f);

  const layout::LayoutSpec restored = json.get<layout::LayoutSpec>();
  REQUIRE(restored.m_views.at(0).m_left == -1.0f);
  REQUIRE(restored.m_views.at(1).m_left == 0.0f);
}

TEST_CASE("layout spec JSON preserves receiver-only sync membership", "[layout][serialization]")
{
  const layout::LayoutSpec original = makePopulatedLayoutSpec();

  const nlohmann::json json = original;
  const auto& receiverOnlyView = json.at("views").at(1);

  REQUIRE(receiverOnlyView.at("sync").at("zoom").is_null());
  REQUIRE(receiverOnlyView.at("syncMembership").at("zoom").get<std::size_t>() == 2);

  const layout::LayoutSpec restored = json.get<layout::LayoutSpec>();
  REQUIRE(restored.m_views.at(1).m_zoomSyncGroup == std::nullopt);
  REQUIRE(restored.m_views.at(1).m_zoomSyncMembershipGroup == 2);
}

TEST_CASE("legacy layout spec JSON without sync membership remains readable", "[layout][serialization]")
{
  const nlohmann::json json = {
    {"views",
     {{{"sync", {{"rotation", 0}, {"translation", nullptr}, {"zoom", 2}}},
       {"viewport", {{"left", -1.0f}, {"bottom", -1.0f}, {"width", 2.0f}, {"height", 2.0f}}}}}}};

  const layout::LayoutSpec restored = json.get<layout::LayoutSpec>();

  REQUIRE(restored.m_views.size() == 1);
  REQUIRE(restored.m_views.front().m_rotationSyncGroup == 0);
  REQUIRE(restored.m_views.front().m_translationSyncGroup == std::nullopt);
  REQUIRE(restored.m_views.front().m_zoomSyncGroup == 2);
  REQUIRE(restored.m_views.front().m_rotationSyncMembershipGroup == std::nullopt);
  REQUIRE(restored.m_views.front().m_translationSyncMembershipGroup == std::nullopt);
  REQUIRE(restored.m_views.front().m_zoomSyncMembershipGroup == std::nullopt);
}

TEST_CASE("missing optional layout spec fields keep defaults", "[layout][serialization]")
{
  const nlohmann::json json = {
    {"isLightbox", false},
    {"views", {{{"viewport", {{"left", -0.5f}, {"bottom", -0.25f}, {"width", 1.0f}, {"height", 0.5f}}}}}}};

  const layout::LayoutSpec restored = json.get<layout::LayoutSpec>();

  REQUIRE(restored.m_isLightbox == false);
  REQUIRE(restored.m_kind == 0);
  REQUIRE(restored.m_renderMode == 0);
  REQUIRE(restored.m_defaultRenderAllImages == false);
  REQUIRE(restored.m_views.size() == 1);
  REQUIRE(restored.m_views.front().m_offsetImageIndex == std::nullopt);
  REQUIRE(restored.m_views.front().m_defaultRenderAllImages == true);
}
