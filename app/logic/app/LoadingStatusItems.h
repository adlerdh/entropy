#pragma once

#include "ui/GuiData.h"

#include <filesystem>
#include <vector>

namespace dicom
{
struct SeriesInfo;
}

namespace serialize
{
struct EntropyProject;
}

namespace loading_status
{
/**
 * @brief Create loading-status rows for plain image file loading.
 * @param fileNames Image file paths to show in the loading popup.
 * @return Loading-status rows in the same order as the input files.
 */
std::vector<GuiData::LoadingStatusItem> imageItems(const std::vector<std::filesystem::path>& fileNames);

/**
 * @brief Create loading-status rows for a serialized project.
 * @param project Project whose image and segmentation files will be loaded.
 * @return Loading-status rows for the reference image, additional images, and segmentations.
 */
std::vector<GuiData::LoadingStatusItem> projectItems(const serialize::EntropyProject& project);

/**
 * @brief Create loading-status rows for selected DICOM series.
 * @param series DICOM series selected for loading.
 * @return Loading-status rows with series file-size totals when available.
 */
std::vector<GuiData::LoadingStatusItem> dicomSeriesItems(const std::vector<dicom::SeriesInfo>& series);

/**
 * @brief Compare loading-status paths using exact, canonical, or filename-only matching.
 * @param a First path.
 * @param b Second path.
 * @return True when the paths identify the same loading item for UI status updates.
 */
bool equivalentPath(const std::filesystem::path& a, const std::filesystem::path& b);
} // namespace loading_status
