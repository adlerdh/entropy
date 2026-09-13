#pragma once

#include "ui/GuiData.h"

#include <cstdint>
#include <string>
#include <vector>

namespace ui::loading_status_model
{

/**
 * @brief Byte-weighted loading progress for the loading-status popup.
 */
struct LoadingProgress
{
  std::uintmax_t loaded = 0; //!< Weighted bytes loaded, or fallback units for unknown-size items
  std::uintmax_t total = 0;  //!< Total weighted bytes, or fallback units for unknown-size items
};

/**
 * @brief Return the progress weight for one loading item.
 *
 * @details Known-size items are weighted by file bytes. Unknown-size items use a single fallback
 * unit so they still contribute to aggregate progress.
 */
std::uintmax_t loadingItemWeight(const GuiData::LoadingStatusItem& item);

/**
 * @brief Compute aggregate byte-weighted progress over all loading rows.
 */
LoadingProgress loadingProgress(const std::vector<GuiData::LoadingStatusItem>& items);

/**
 * @brief Convert loading progress to the [0, 1] fraction expected by ImGui::ProgressBar.
 */
float progressFraction(const LoadingProgress& progress);

/**
 * @brief Format a compact percentage label for the progress bar.
 */
std::string progressPercentLabel(float fraction);

} // namespace ui::loading_status_model
