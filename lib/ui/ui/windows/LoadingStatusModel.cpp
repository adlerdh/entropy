#include "ui/windows/LoadingStatusModel.h"

#include <algorithm>
#include <cmath>

namespace ui::loading_status_model
{

std::uintmax_t loadingItemWeight(const GuiData::LoadingStatusItem& item)
{
  return item.bytes.value_or(1u);
}

LoadingProgress loadingProgress(const std::vector<GuiData::LoadingStatusItem>& items)
{
  LoadingProgress progress;
  for (const GuiData::LoadingStatusItem& item : items) {
    const std::uintmax_t weight = loadingItemWeight(item);
    progress.total += weight;
    if (item.loaded) {
      progress.loaded += weight;
    }
  }
  return progress;
}

float progressFraction(const LoadingProgress& progress)
{
  if (0 == progress.total) {
    return 0.0f;
  }
  return static_cast<float>(
    std::clamp(static_cast<double>(progress.loaded) / static_cast<double>(progress.total), 0.0, 1.0));
}

std::string progressPercentLabel(float fraction)
{
  const int percent = static_cast<int>(std::round(100.0f * std::clamp(fraction, 0.0f, 1.0f)));
  return std::to_string(percent) + "%";
}

} // namespace ui::loading_status_model
