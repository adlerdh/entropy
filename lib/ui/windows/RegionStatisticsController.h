#pragma once

#include "image/RegionStatistics.h"

#include <uuid.h>

#include <cstdint>
#include <array>
#include <optional>
#include <string>
#include <vector>

class Image;

struct RegionStatisticsCalculationKey
{
  uuids::uuid imageUid{};
  uuids::uuid segmentationUid{};
  RegionValueSelection value{};
  std::uint32_t timePoint = 0;
  std::uint64_t imageRevision = 0;
  std::uint64_t segmentationRevision = 0;
  std::uint64_t segmentationGeometryRevision = 0;
  bool computeQuartiles = false;

  friend bool operator==(const RegionStatisticsCalculationKey&, const RegionStatisticsCalculationKey&) = default;
};

/// Owns selection and derived-data state for the segmentation region statistics window.
class RegionStatisticsController
{
public:
  void requestSelection(
    std::optional<uuids::uuid> requestedImageUid,
    std::optional<uuids::uuid> requestedSegmentationUid);
  void setImage(std::optional<uuids::uuid> selectedImageUid, std::optional<uuids::uuid> defaultSegmentationUid);
  void setSegmentation(std::optional<uuids::uuid> selectedSegmentationUid);
  void setValueSelection(RegionValueSelection selection);
  void setComputeQuartiles(bool enabled);
  bool synchronizeEmptyLabelRows(std::size_t knownLabelCount);
  void invalidate();

  void updateStatistics(
    const RegionStatisticsCalculationKey& key,
    const Image& image,
    const Image& segmentation,
    bool defer);

  void updateHistogram(
    const RegionStatisticsCalculationKey& key,
    const Image& image,
    const Image& segmentation,
    std::optional<std::int64_t> label,
    const RegionHistogramOptions& options);

  std::optional<uuids::uuid> imageUid;
  std::optional<uuids::uuid> segmentationUid;
  RegionValueSelection value;
  bool includeBackground = false;
  bool includeEmptyLabels = false;
  bool computeQuartiles = false;
  bool histogramWholeImage = false;
  int histogramBins = 128;
  bool histogramCumulative = false;
  bool histogramDensity = false;
  bool histogramHorizontal = false;
  bool histogramLogScale = false;
  bool histogramUseCustomRange = false;
  bool histogramOpen = false;
  std::optional<std::array<double, 2>> histogramDataRange;
  std::optional<std::array<double, 2>> histogramValueRange;
  std::optional<std::int64_t> selectedLabel;
  std::optional<RegionStatisticsResult> result;
  std::vector<RegionStatistic> sortedRegions;
  std::optional<RegionHistogram> histogram;
  std::string status;

private:
  std::optional<RegionStatisticsCalculationKey> m_calculatedKey;
  std::optional<RegionStatisticsCalculationKey> m_histogramKey;
  std::optional<std::int64_t> m_histogramLabel;
  std::optional<RegionHistogramOptions> m_histogramOptions;
};
