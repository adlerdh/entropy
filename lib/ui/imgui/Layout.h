#pragma once

/// @file Docking layout, viewport sizing, and layout-tab controls.

#include <imgui/imgui.h>
#include <filesystem>
#include <string>

class AppData;
struct GuiData;
class Layout;

namespace ui::imgui_detail
{
/// @brief Update tab-bar height and spacing from the current ImGui style; return whether either changed.
bool updateLayoutTabBarHeight(GuiData& guiData);

/// @brief Check for Entropy dockspace entries in an ImGui settings file; return false if unreadable.
bool savedDockspaceLayoutExists(const std::filesystem::path& iniFilePath);

/// @brief Draw the main dockspace and return its ID, or zero if no main viewport exists.
ImGuiID renderMainDockspace(const GuiData& guiData);

/// @brief Reveal tabs throughout the dockspace; return whether any node flags changed.
bool keepDockTabsVisible(ImGuiID dockspaceId);

/// @brief Update or clear the rendering viewport from the central dock node; return whether it changed.
bool updateRenderViewportFromDockspace(AppData& appData, ImGuiID dockspaceId);

/// @brief Rebuild the default panel arrangement; a zero dockspace ID is ignored.
void applyDefaultPanelDockLayout(ImGuiID dockspaceId, const AppData& appData);

/// @brief Return the first rendered image's display name, or an empty string if unavailable.
std::string layoutImageDisplayName(const AppData& appData, const Layout& layout);

/// @brief Copy tab visibility and placement settings into the GUI state.
void syncLayoutTabGuiDataFromSettings(AppData& appData);

/// @brief Request removal confirmation for a zero-based layout index, preserving at least one layout.
void requestLayoutRemoval(AppData& appData, std::size_t index);

/// @brief Show pending layout-removal confirmation and apply the user's decision.
void renderConfirmRemoveLayoutPopup(AppData& appData);

/// @brief Draw enabled layout tabs and handle selection, reordering, and removal requests.
void renderLayoutTabs(AppData& appData);
} // namespace ui::imgui_detail
