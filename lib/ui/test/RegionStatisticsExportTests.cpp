#include "ui/RegionStatisticsExport.h"

#include <catch2/catch_test_macros.hpp>

#include <string>

TEST_CASE("Region statistics exports include optional quartiles", "[ui][statistics][export]")
{
  RegionStatistic statistic{
    .label = 2,
    .elementCount = 4,
    .finiteValueCount = 4,
    .physicalSize = 8.0,
    .minimum = 1.0,
    .mean = 2.5,
    .maximum = 4.0,
    .standardDeviation = 1.25,
    .firstQuartile = 1.75,
    .median = 2.5,
    .thirdQuartile = 3.25};
  const RegionStatisticsDocument document{
    .imageName = "Image, one",
    .imageValueName = "Intensity",
    .timePoint = 0,
    .segmentationName = "Segmentation",
    .isVolume = true,
    .includesQuartiles = true,
    .regions = {{.statistic = statistic, .name = "Region two"}}};

  const std::string csv = regionStatisticsDelimitedText(document, ',');
  CHECK(csv.find("Q1,Median,Q3") != std::string::npos);
  CHECK(csv.find("\"Image, one\"") != std::string::npos);

  const std::string json = regionStatisticsJson(document);
  CHECK(json.find("\"firstQuartile\": 1.75") != std::string::npos);
  CHECK(json.find("\"median\": 2.5") != std::string::npos);
}

TEST_CASE("Region statistics exports omit disabled quartiles", "[ui][statistics][export]")
{
  const RegionStatisticsDocument document{
    .imageName = "Image",
    .imageValueName = "Intensity",
    .segmentationName = "Segmentation",
    .includesQuartiles = false,
    .regions = {{.statistic = {.label = 1, .elementCount = 1, .finiteValueCount = 1}, .name = "Region"}}};

  CHECK(regionStatisticsDelimitedText(document, ',').find("Median") == std::string::npos);
  CHECK(regionStatisticsJson(document).find("firstQuartile") == std::string::npos);
}
