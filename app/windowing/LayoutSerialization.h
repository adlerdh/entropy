#pragma once

#include "common/UuidRange.h"
#include "windowing/Layout.h"
#include "windowing/LayoutSpec.h"

#include <optional>
#include <vector>

struct CrosshairsState;

namespace layout
{

LayoutSpec createLayoutSpec(const Layout& layout, uuid_range_t orderedImageUids);
std::vector<LayoutSpec> createLayoutSpecs(const std::vector<Layout>& layouts, uuid_range_t orderedImageUids);

Layout instantiateLayoutSpec(
  const LayoutSpec& spec,
  uuid_range_t orderedImageUids,
  const CrosshairsState& crosshairs,
  const ViewAlignmentMode& viewAlignment,
  const ViewConvention& viewConvention);

std::vector<Layout> instantiateLayoutSpecs(
  const std::vector<LayoutSpec>& specs,
  uuid_range_t orderedImageUids,
  const CrosshairsState& crosshairs,
  const ViewAlignmentMode& viewAlignment,
  const ViewConvention& viewConvention);

} // namespace layout
