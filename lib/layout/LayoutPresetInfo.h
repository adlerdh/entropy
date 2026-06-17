#pragma once

#include "layout/LayoutPreset.h"
#include "viewer/ViewTypes.h"

#include <cstddef>
#include <optional>
#include <string_view>
#include <vector>

namespace layout
{

/**
 * @brief Serialized preset name for a view type.
 * @param viewType View type to encode.
 * @return Stable view name used in layout preset files.
 */
std::string_view layoutPresetViewName(ViewType viewType);

/**
 * @brief View type represented by a serialized preset view name.
 * @param name View name read from a layout preset.
 * @return Matching view type, or std::nullopt when the name is unsupported.
 */
std::optional<ViewType> layoutPresetViewType(std::string_view name);

/**
 * @brief Explicit preset image indices, defaulting to the first image.
 * @param preset Preset whose image indices should be read.
 * @return Stored indices, or `{0}` when the preset does not specify any.
 */
std::vector<std::size_t> presetImageIndicesOrDefault(const LayoutPreset& preset);

} // namespace layout
