#pragma once

#include "common/Types.h"
#include "common/UuidRange.h"
#include "layout/LayoutSpec.h"
#include "logic/app/CrosshairsState.h"
#include "windowing/Layout.h"

#include <vector>

namespace layout
{

/**
 * @brief Create a serializable layout snapshot.
 * @param layout Layout to snapshot.
 * @param orderedImageUids Image UIDs in application order.
 * @return Layout specification.
 */
LayoutSpec createLayoutSpec(const Layout& layout, uuid_range_t orderedImageUids);

/**
 * @brief Create serializable layout snapshots.
 * @param layouts Layouts to snapshot.
 * @param orderedImageUids Image UIDs in application order.
 * @return Layout specifications in display order.
 */
std::vector<LayoutSpec> createLayoutSpecs(const std::vector<Layout>& layouts, uuid_range_t orderedImageUids);

/**
 * @brief Build a layout from a snapshot.
 * @param spec Layout specification.
 * @param orderedImageUids Image UIDs in application order.
 * @param crosshairs Crosshairs state used by created views.
 * @param viewAlignment View alignment mode.
 * @param viewConvention View orientation convention.
 * @return Layout built from `spec`.
 */
Layout instantiateLayoutSpec(
  const LayoutSpec& spec,
  uuid_range_t orderedImageUids,
  const CrosshairsState& crosshairs,
  const ViewAlignmentMode& viewAlignment,
  const ViewConvention& viewConvention);

/**
 * @brief Build layouts from snapshots.
 * @param specs Layout specifications.
 * @param orderedImageUids Image UIDs in application order.
 * @param crosshairs Crosshairs state used by created views.
 * @param viewAlignment View alignment mode.
 * @param viewConvention View orientation convention.
 * @return Layouts built from `specs`.
 */
std::vector<Layout> instantiateLayoutSpecs(
  const std::vector<LayoutSpec>& specs,
  uuid_range_t orderedImageUids,
  const CrosshairsState& crosshairs,
  const ViewAlignmentMode& viewAlignment,
  const ViewConvention& viewConvention);

} // namespace layout
