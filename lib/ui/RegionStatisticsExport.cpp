#include "ui/RegionStatisticsExport.h"

#include <nlohmann/json.hpp>

#include <iomanip>
#include <sstream>
#include <string>

namespace
{
std::string quoteCsv(const std::string& value)
{
  if (value.find_first_of(",\"\n\r") == std::string::npos) return value;
  std::string quoted = "\"";
  for (const char character : value) {
    if ('\"' == character) quoted += '\"';
    quoted += character;
  }
  return quoted + '"';
}
} // namespace

std::string regionStatisticsDelimitedText(const RegionStatisticsDocument& document, char delimiter)
{
  std::ostringstream stream;
  stream << "Image" << delimiter << "Image value" << delimiter << "Time point" << delimiter << "Segmentation"
         << delimiter << "Label" << delimiter << "Name" << delimiter
         << (document.isVolume ? "Voxel count" : "Pixel count") << delimiter
         << (document.isVolume ? "Volume (mm^3)" : "Area (mm^2)") << delimiter << "Finite values" << delimiter
         << "Excluded non-finite" << delimiter << "Minimum";
  if (document.includesQuartiles) stream << delimiter << "Q1" << delimiter << "Median" << delimiter << "Q3";
  stream << delimiter << "Maximum" << delimiter << "Mean" << delimiter << "Standard deviation\n";
  stream << std::setprecision(17);

  const auto quote = [delimiter](const std::string& value) {
    return ',' == delimiter ? quoteCsv(value) : value;
  };
  for (const auto& record : document.regions) {
    const RegionStatistic& region = record.statistic;
    stream << quote(document.imageName) << delimiter << quote(document.imageValueName) << delimiter
           << document.timePoint << delimiter << quote(document.segmentationName) << delimiter << region.label
           << delimiter << quote(record.name) << delimiter << region.elementCount << delimiter << region.physicalSize
           << delimiter << region.finiteValueCount << delimiter << region.nonFiniteValueCount << delimiter
           << region.minimum;
    if (document.includesQuartiles) {
      stream << delimiter << region.firstQuartile.value_or(0.0) << delimiter << region.median.value_or(0.0) << delimiter
             << region.thirdQuartile.value_or(0.0);
    }
    stream << delimiter << region.maximum << delimiter << region.mean << delimiter << region.standardDeviation << '\n';
  }
  return stream.str();
}

std::string regionStatisticsJson(const RegionStatisticsDocument& document)
{
  nlohmann::json regions = nlohmann::json::array();
  for (const auto& record : document.regions) {
    const RegionStatistic& region = record.statistic;
    nlohmann::json entry{
      {"label", region.label},
      {"name", record.name},
      {document.isVolume ? "voxelCount" : "pixelCount", region.elementCount},
      {document.isVolume ? "volumeMm3" : "areaMm2", region.physicalSize},
      {"finiteValueCount", region.finiteValueCount},
      {"excludedNonFiniteValueCount", region.nonFiniteValueCount},
      {"minimum", region.minimum},
      {"mean", region.mean},
      {"maximum", region.maximum},
      {"populationStandardDeviation", region.standardDeviation}};
    if (document.includesQuartiles) {
      entry["firstQuartile"] = region.firstQuartile.value_or(0.0);
      entry["median"] = region.median.value_or(0.0);
      entry["thirdQuartile"] = region.thirdQuartile.value_or(0.0);
    }
    regions.push_back(std::move(entry));
  }
  return nlohmann::json{
           {"image", document.imageName},
           {"imageValue", document.imageValueName},
           {"timePoint", document.timePoint},
           {"segmentation", document.segmentationName},
           {"regions", std::move(regions)}}
           .dump(2) +
         '\n';
}
