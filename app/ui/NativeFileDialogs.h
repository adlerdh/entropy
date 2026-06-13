#pragma once

#include <filesystem>

#include <optional>
#include <string>
#include <vector>

namespace native_dialog
{
/**
 * @brief File type filter passed to native open/save dialogs.
 */
struct Filter
{
  std::string name;       //!< Human-readable filter name shown by the platform dialog.
  std::string extensions; //!< Comma-separated file extensions accepted by this filter.
};

/**
 * @brief Show a native single-file open dialog.
 *
 * @param filters Optional file type filters.
 * @param defaultPath Optional initial directory or file path.
 * @return Selected file path, or std::nullopt when canceled or unavailable.
 */
std::optional<std::filesystem::path> openFile(
  const std::vector<Filter>& filters = {},
  const std::filesystem::path& defaultPath = {});

/**
 * @brief Show a native multi-file open dialog.
 *
 * @param filters Optional file type filters.
 * @param defaultPath Optional initial directory or file path.
 * @return Selected file paths; empty when canceled or unavailable.
 */
std::vector<std::filesystem::path> openFiles(
  const std::vector<Filter>& filters = {},
  const std::filesystem::path& defaultPath = {});

/**
 * @brief Show a native folder picker.
 *
 * @param defaultPath Optional initial folder.
 * @return Selected folder, or std::nullopt when canceled or unavailable.
 */
std::optional<std::filesystem::path> pickFolder(const std::filesystem::path& defaultPath = {});

/**
 * @brief Show a native multi-folder picker.
 *
 * @param defaultPath Optional initial folder.
 * @return Selected folders; empty when canceled or unavailable.
 */
std::vector<std::filesystem::path> pickFolders(const std::filesystem::path& defaultPath = {});

/**
 * @brief Show a native save-file dialog.
 *
 * @param filters Optional file type filters.
 * @param defaultPath Optional initial directory or file path.
 * @param defaultName Optional default file name.
 * @return Selected save path, or std::nullopt when canceled or unavailable.
 */
std::optional<std::filesystem::path> saveFile(
  const std::vector<Filter>& filters = {},
  const std::filesystem::path& defaultPath = {},
  const std::string& defaultName = {});

/**
 * @brief Filters for image files that Entropy can load.
 */
std::vector<Filter> imageFilters();

/**
 * @brief Filters for medical image files that Entropy can export.
 */
std::vector<Filter> medicalImageExportFilters();

/**
 * @brief Filters for Entropy project files.
 */
std::vector<Filter> projectFilters();

/**
 * @brief Filters for Entropy layout preset files.
 */
std::vector<Filter> layoutFilters();

/**
 * @brief Filters for image transformation files.
 */
std::vector<Filter> transformFilters();

/**
 * @brief Filters for segmentation image files.
 */
std::vector<Filter> segmentationFilters();

/**
 * @brief Filters for landmark files.
 */
std::vector<Filter> landmarkFilters();

/**
 * @brief Filters for annotation files.
 */
std::vector<Filter> annotationFilters();
} // namespace native_dialog
