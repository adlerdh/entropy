#include "logic/app/ProjectLayoutDelta.h"

#include <catch2/catch_test_macros.hpp>

#include <vector>

namespace
{

layout::LayoutSpec layoutWithKind(const int kind)
{
  layout::LayoutSpec layout;
  layout.m_kind = kind;
  layout.m_grid = layout::GridSpec{};
  return layout;
}

} // namespace

TEST_CASE("project layout delta is empty for unchanged generated layouts", "[project][layouts]")
{
  const std::vector<layout::LayoutSpec> defaults{layoutWithKind(1), layoutWithKind(2)};

  const auto delta = project_layout_delta::compactLayoutDelta(defaults, defaults);

  REQUIRE(delta);
  CHECK(delta->m_addedLayouts.empty());
  CHECK(delta->m_removedDefaultLayoutIndices.empty());
  CHECK(delta->m_modifiedDefaultLayouts.empty());
}

TEST_CASE("project layout delta records deleted generated layouts", "[project][layouts]")
{
  const std::vector<layout::LayoutSpec> defaults{layoutWithKind(1), layoutWithKind(2), layoutWithKind(3)};
  const std::vector<layout::LayoutSpec> current{defaults.at(0), defaults.at(2)};

  const auto delta = project_layout_delta::compactLayoutDelta(current, defaults);

  REQUIRE(delta);
  CHECK(delta->m_addedLayouts.empty());
  CHECK(delta->m_removedDefaultLayoutIndices == std::vector<std::size_t>{1});
  CHECK(delta->m_modifiedDefaultLayouts.empty());
}

TEST_CASE("project layout delta records edited generated layouts as overrides", "[project][layouts]")
{
  const std::vector<layout::LayoutSpec> defaults{layoutWithKind(1), layoutWithKind(2)};
  std::vector<layout::LayoutSpec> current = defaults;
  current.at(0).m_views.push_back(layout::ViewSpec{.m_imageSelection = {.m_renderedImageIndices = {0}}});

  const auto delta = project_layout_delta::compactLayoutDelta(current, defaults);

  REQUIRE(delta);
  CHECK(delta->m_addedLayouts.empty());
  CHECK(delta->m_removedDefaultLayoutIndices.empty());
  REQUIRE(delta->m_modifiedDefaultLayouts.size() == 1);
  CHECK(delta->m_modifiedDefaultLayouts.front().m_index == 0);
  CHECK(delta->m_modifiedDefaultLayouts.front().m_layout == current.front());
}

TEST_CASE("project layout delta records project layouts appended after generated layouts", "[project][layouts]")
{
  const std::vector<layout::LayoutSpec> defaults{layoutWithKind(1), layoutWithKind(2)};
  const layout::LayoutSpec customLayout = layoutWithKind(0);
  const std::vector<layout::LayoutSpec> current{defaults.at(0), defaults.at(1), customLayout};

  const auto delta = project_layout_delta::compactLayoutDelta(current, defaults);

  REQUIRE(delta);
  REQUIRE(delta->m_addedLayouts.size() == 1);
  CHECK(delta->m_addedLayouts.front() == customLayout);
  CHECK(delta->m_removedDefaultLayoutIndices.empty());
  CHECK(delta->m_modifiedDefaultLayouts.empty());
}
