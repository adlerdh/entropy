#pragma once

#include <filesystem>

#include <optional>
#include <string>
#include <vector>

namespace native_dialog
{
struct Filter
{
  std::string name;
  std::string extensions;
};

std::optional<std::filesystem::path> openFile(
  const std::vector<Filter>& filters = {},
  const std::filesystem::path& defaultPath = {});
std::vector<std::filesystem::path> openFiles(
  const std::vector<Filter>& filters = {},
  const std::filesystem::path& defaultPath = {});
/// @brief Show a native folder picker.
/// @param defaultPath Optional initial folder.
/// @return Selected folder, or `std::nullopt` when cancelled or unavailable.
std::optional<std::filesystem::path> pickFolder(const std::filesystem::path& defaultPath = {});

/// @brief Show a native multi-folder picker.
/// @param defaultPath Optional initial folder.
/// @return Selected folders; empty when cancelled or unavailable.
std::vector<std::filesystem::path> pickFolders(const std::filesystem::path& defaultPath = {});
std::optional<std::filesystem::path> saveFile(
  const std::vector<Filter>& filters = {},
  const std::filesystem::path& defaultPath = {},
  const std::string& defaultName = {});

std::vector<Filter> imageFilters();
std::vector<Filter> medicalImageExportFilters();
std::vector<Filter> projectFilters();
std::vector<Filter> layoutFilters();
std::vector<Filter> transformFilters();
std::vector<Filter> segmentationFilters();
std::vector<Filter> landmarkFilters();
std::vector<Filter> annotationFilters();
} // namespace native_dialog
