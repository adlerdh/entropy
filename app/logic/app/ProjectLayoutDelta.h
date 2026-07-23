#pragma once

#include "layout/LayoutSpec.h"
#include "logic/serialization/ProjectSerialization.h"

#include <cstddef>
#include <optional>
#include <vector>

namespace project_layout_delta
{

/**
 * @brief Compact difference between generated default layouts and the current project layouts.
 */
struct CompactLayoutDelta
{
  std::vector<layout::LayoutSpec> m_addedLayouts;                         //!< Layouts appended by the user
  std::vector<std::size_t> m_removedDefaultLayoutIndices;                 //!< Generated layouts removed by the user
  std::vector<serialize::DefaultLayoutOverride> m_modifiedDefaultLayouts; //!< Generated layouts edited by the user
};

/**
 * @brief Represent current layouts as edits to generated defaults plus appended project layouts.
 * @param currentLayouts Current layout snapshots in display order.
 * @param defaultLayouts Generated default layout snapshots in display order.
 * @return Compact delta, or null when the layout order cannot be represented safely.
 */
std::optional<CompactLayoutDelta> compactLayoutDelta(
  const std::vector<layout::LayoutSpec>& currentLayouts,
  const std::vector<layout::LayoutSpec>& defaultLayouts);

} // namespace project_layout_delta
