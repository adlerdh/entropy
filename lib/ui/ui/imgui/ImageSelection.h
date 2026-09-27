#pragma once

/// @file Image-selection helpers shared by ImGui menus and controls.

#include <uuid.h>

class AppData;

namespace ui::imgui_detail
{
/// @brief Check whether the given image is the only image after excluding other warp candidates.
bool imageIsOnlyNonWarpImage(const AppData& appData, const uuids::uuid& imageUid);
} // namespace ui::imgui_detail
