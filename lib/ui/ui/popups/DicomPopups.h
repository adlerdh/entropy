#pragma once

#include "ui/GuiData.h"

#include <uuid.h>

#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <vector>

class AppData;

/**
 * @brief Render fallback DICOM folder-entry UI when native folder picking is unavailable.
 */
void renderDicomFolderPathPopup(
  AppData& appData,
  const std::function<void(const std::vector<std::filesystem::path>& folderNames)>& openDicomFolders);

/**
 * @brief Render DICOM scan progress, series selection, and metadata inspection popups.
 */
void renderDicomSeriesSelectionPopup(
  AppData& appData,
  const std::function<void(
    const std::vector<dicom::SeriesInfo>& series,
    std::optional<std::size_t> referenceSeriesIndex,
    bool addToExistingProject)>& loadSelectedSeries);
