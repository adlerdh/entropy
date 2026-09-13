#pragma once

#include "image/RegionStatistics.h"

#include <cstdint>
#include <string>
#include <vector>

struct RegionStatisticsRecord
{
  RegionStatistic statistic;
  std::string name;
};

struct RegionStatisticsDocument
{
  std::string imageName;
  std::string imageValueName;
  std::uint32_t timePoint = 0;
  std::string segmentationName;
  bool isVolume = true;
  bool includesQuartiles = false;
  std::vector<RegionStatisticsRecord> regions;
};

/** Serialize a statistics document as CSV or tab-separated text. */
std::string regionStatisticsDelimitedText(const RegionStatisticsDocument& document, char delimiter);

/** Serialize a statistics document as formatted JSON. */
std::string regionStatisticsJson(const RegionStatisticsDocument& document);
