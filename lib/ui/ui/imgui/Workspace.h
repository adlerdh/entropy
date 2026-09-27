#pragma once

/// @file Welcome-screen actions and background-work status windows.

#include "logic/app/State.h"

#include <filesystem>
#include <functional>
#include <vector>

class AppSettings;
struct GuiData;

namespace ui::imgui_detail
{
/// @brief Draw loading progress when a visible loading-status record exists.
void renderLoadingStatusWindow(const GuiData& guiData);

/// @brief Draw mesh-extraction progress when a visible status record exists.
void renderMeshExtractionStatusWindow(const GuiData& guiData);

/**
 * @brief Draw the welcome screen for an empty or failed project.
 * File-opening and recent-history actions are delegated to the supplied callbacks.
 */
void renderEmptyWorkspace(
  ProjectLoadState projectLoadState,
  const AppSettings& settings,
  const std::function<void(const std::vector<std::filesystem::path>&)>& openImageFiles,
  const std::function<void(const std::vector<std::filesystem::path>&)>& openDicomFolders,
  const std::function<void()>& requestDicomFolderPathDialog,
  const std::function<void(const std::filesystem::path&)>& openProjectFile,
  const std::function<void()>& clearRecents);

} // namespace ui::imgui_detail
