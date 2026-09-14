#pragma once

#include <filesystem>
#include <optional>
#include <string>

namespace window_title
{
/**
 * @brief Return the project name used in the window title.
 * @param projectFileName Project file path.
 * @return Project filename without its parent path.
 */
std::string projectDisplayName(const std::filesystem::path& projectFileName);

/**
 * @brief Compose the status text inserted into the Entropy window title.
 * @param projectFileName Optional project file path.
 * @param imageDisplayNames Comma-separated image display names.
 * @param dirty True when the loaded project has unsaved changes.
 * @return Saved project filename, or image display names when the project has no file.
 */
std::string
status(const std::optional<std::filesystem::path>& projectFileName, const std::string& imageDisplayNames, bool dirty);
} // namespace window_title
