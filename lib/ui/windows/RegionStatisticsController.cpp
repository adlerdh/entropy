#include "ui/windows/RegionStatisticsController.h"

#include "image/Image.h"

#include <algorithm>
#include <utility>

void RegionStatisticsController::requestSelection(
  std::optional<uuids::uuid> requestedImageUid,
  std::optional<uuids::uuid> requestedSegmentationUid)
{
  imageUid = requestedImageUid;
  segmentationUid = requestedSegmentationUid;
  value = {};
  histogramDataRange.reset();
  histogramValueRange.reset();
  invalidate();
}

void RegionStatisticsController::setImage(
  std::optional<uuids::uuid> selectedImageUid,
  std::optional<uuids::uuid> defaultSegmentationUid)
{
  if (imageUid == selectedImageUid && segmentationUid == defaultSegmentationUid) return;
  imageUid = selectedImageUid;
  segmentationUid = defaultSegmentationUid;
  value = {};
  histogramDataRange.reset();
  histogramValueRange.reset();
  invalidate();
}

void RegionStatisticsController::setSegmentation(std::optional<uuids::uuid> selectedSegmentationUid)
{
  if (segmentationUid == selectedSegmentationUid) return;
  segmentationUid = selectedSegmentationUid;
  histogramDataRange.reset();
  histogramValueRange.reset();
  invalidate();
}

void RegionStatisticsController::setValueSelection(RegionValueSelection selection)
{
  if (value == selection) return;
  value = selection;
  histogramDataRange.reset();
  histogramValueRange.reset();
  invalidate();
}

void RegionStatisticsController::setComputeQuartiles(bool enabled)
{
  if (computeQuartiles == enabled) return;
  computeQuartiles = enabled;
  invalidate();
}

bool RegionStatisticsController::synchronizeEmptyLabelRows(std::size_t knownLabelCount)
{
  if (!result) return false;
  bool changed = false;
  if (!includeEmptyLabels) {
    const auto firstEmpty =
      std::remove_if(sortedRegions.begin(), sortedRegions.end(), [](const RegionStatistic& region) {
        return 0 == region.elementCount;
      });
    changed = firstEmpty != sortedRegions.end();
    sortedRegions.erase(firstEmpty, sortedRegions.end());
    return changed;
  }

  for (std::size_t label = 0; label < knownLabelCount; ++label) {
    const auto signedLabel = static_cast<std::int64_t>(label);
    const bool exists =
      std::any_of(sortedRegions.begin(), sortedRegions.end(), [signedLabel](const RegionStatistic& region) {
        return region.label == signedLabel;
      });
    if (!exists) {
      sortedRegions.push_back({.label = signedLabel});
      changed = true;
    }
  }
  return changed;
}

void RegionStatisticsController::invalidate()
{
  m_calculatedKey.reset();
  m_histogramKey.reset();
  m_histogramOptions.reset();
  result.reset();
  histogram.reset();
}

void RegionStatisticsController::updateStatistics(
  const RegionStatisticsCalculationKey& key,
  const Image& image,
  const Image& segmentation,
  bool defer)
{
  if (m_calculatedKey == key) return;
  if (defer) {
    status = "Updates after the current edit";
    return;
  }

  auto calculation =
    computeRegionStatistics(image, segmentation, value, key.timePoint, {.computeQuartiles = computeQuartiles});
  m_calculatedKey = key;
  m_histogramKey.reset();
  histogram.reset();
  if (!calculation) {
    result.reset();
    sortedRegions.clear();
    status = regionStatisticsErrorMessage(calculation.error());
    return;
  }

  result = std::move(*calculation);
  sortedRegions = result->regions;
  const auto firstFinite = std::find_if(sortedRegions.begin(), sortedRegions.end(), [](const RegionStatistic& region) {
    return region.finiteValueCount > 0;
  });
  if (firstFinite != sortedRegions.end()) {
    double minimum = firstFinite->minimum;
    double maximum = firstFinite->maximum;
    for (const auto& region : sortedRegions) {
      if (0 == region.finiteValueCount) continue;
      minimum = std::min(minimum, region.minimum);
      maximum = std::max(maximum, region.maximum);
    }
    if (minimum == maximum) maximum = minimum + 1.0;
    histogramDataRange = std::array<double, 2>{minimum, maximum};
    if (!histogramUseCustomRange || !histogramValueRange) histogramValueRange = histogramDataRange;
  }
  if (selectedLabel && std::none_of(sortedRegions.begin(), sortedRegions.end(), [&](const RegionStatistic& region) {
        return region.label == *selectedLabel;
      }))
  {
    selectedLabel.reset();
  }
  if (!selectedLabel) {
    const auto firstForeground =
      std::find_if(sortedRegions.begin(), sortedRegions.end(), [](const RegionStatistic& region) {
        return 0 != region.label;
      });
    if (firstForeground != sortedRegions.end()) selectedLabel = firstForeground->label;
  }
  status = computeQuartiles ? "Statistics are updated with exact quartiles"
                            : "Statistics are updated from current image and segmentation data";
}

void RegionStatisticsController::updateHistogram(
  const RegionStatisticsCalculationKey& key,
  const Image& image,
  const Image& segmentation,
  std::optional<std::int64_t> label,
  const RegionHistogramOptions& options)
{
  if (m_histogramKey == key && m_histogramLabel == label && m_histogramOptions == options) return;
  auto calculation = computeRegionHistogram(image, segmentation, value, key.timePoint, label, options);
  m_histogramKey = key;
  m_histogramLabel = label;
  m_histogramOptions = options;
  if (calculation) {
    histogram = std::move(*calculation);
  }
  else {
    histogram.reset();
    status = regionStatisticsErrorMessage(calculation.error());
  }
}
