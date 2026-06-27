#include "image/DicomSeries.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>

TEST_CASE("DICOM metadata filtering excludes PHI tags", "[image][dicom]")
{
  CHECK(dicom::isPhiTag("0010|0010"));
  CHECK(dicom::isPhiTag("0010|0020"));
  CHECK_FALSE(dicom::includeMetadataTag("0010|0010"));
  CHECK_FALSE(dicom::includeMetadataTag("0010|0020"));
}

TEST_CASE("DICOM metadata filtering excludes private tags by default", "[image][dicom]")
{
  CHECK(dicom::isPrivateTag("0019|100a"));
  CHECK_FALSE(dicom::isPrivateTag("0020|000e"));
  CHECK_FALSE(dicom::includeMetadataTag("0019|100a"));
  CHECK(dicom::includeMetadataTag("0019|100a", true));
  CHECK(dicom::includeMetadataTag("0020|000e"));
}

TEST_CASE("DICOM series display names use stable fallbacks", "[image][dicom]")
{
  dicom::SeriesMetadata metadata;
  metadata.seriesDescription = "Axial T1";
  metadata.protocolName = "Protocol";
  metadata.modality = "MR";
  metadata.seriesNumber = "4";
  CHECK(dicom::displayNameForSeries(metadata, "1.2.3") == "Axial T1");

  metadata.seriesDescription.clear();
  CHECK(dicom::displayNameForSeries(metadata, "1.2.3") == "Protocol");

  metadata.protocolName.clear();
  CHECK(dicom::displayNameForSeries(metadata, "1.2.3") == "MR Series 4");

  metadata.modality.clear();
  metadata.seriesNumber.clear();
  CHECK(dicom::displayNameForSeries(metadata, "1.2.840.113619.2.55.3.604688435.1234") == "DICOM 604688435.1234");
}

TEST_CASE("DICOM series loadable state requires files", "[image][dicom]")
{
  dicom::SeriesInfo info;
  CHECK_FALSE(info.loadable());

  info.files.emplace_back("slice.dcm");
  CHECK(info.loadable());

  info.warnings.emplace_back("warning");
  CHECK(info.loadable());
}

TEST_CASE("DICOM temporal metadata identifies time series", "[image][dicom][time]")
{
  dicom::SeriesTemporalInfo single;
  CHECK_FALSE(single.isTimeSeries());

  dicom::SeriesTemporalInfo cine;
  cine.numTimePoints = 12;
  cine.spacing = 33.3;
  cine.units = "ms";
  cine.multiframe = true;
  CHECK(cine.isTimeSeries());
}

TEST_CASE("DICOM metadata tag names use readable labels", "[image][dicom]")
{
  CHECK(dicom::metadataTagName("0010|0010") == "Patient Name");
  CHECK(dicom::metadataTagName("0020|000d") == "Study Instance UID");
  CHECK(dicom::metadataTagName("0020|000E") == "Series Instance UID");
  CHECK(dicom::metadataTagName("7777|7777") == "Unknown");
}

TEST_CASE("DICOM metadata entries preserve tag name and value", "[image][dicom]")
{
  dicom::MetadataEntry entry{.tag = "0008|0060", .name = dicom::metadataTagName("0008|0060"), .value = "MR"};

  CHECK(entry.tag == "0008|0060");
  CHECK(entry.name == "Modality");
  CHECK(entry.value == "MR");
}

TEST_CASE("DICOM metadata filtering normalizes tag case after successful values", "[image][dicom]")
{
  const std::vector<dicom::RawMetadataEntry> entries{
    {.tag = "0008|103E", .value = "Series from dictionary key"},
    {.tag = "0008|103e", .value = "Duplicate lower-case key"}};

  const auto filtered = dicom::filteredMetadataEntries(entries);

  REQUIRE(filtered.size() == 1);
  CHECK(filtered.front().tag == "0008|103e");
  CHECK(filtered.front().name == "Series Description");
  CHECK(filtered.front().value == "Series from dictionary key");
}

TEST_CASE("DICOM metadata filtering skips empty values before de-duplication", "[image][dicom]")
{
  const std::vector<dicom::RawMetadataEntry> entries{
    {.tag = "0008|103e", .value = ""},
    {.tag = "0008|103E", .value = "Series Description"}};

  const auto filtered = dicom::filteredMetadataEntries(entries);

  REQUIRE(filtered.size() == 1);
  CHECK(filtered.front().tag == "0008|103e");
  CHECK(filtered.front().value == "Series Description");
}

TEST_CASE("DICOM metadata filtering applies PHI and private tag policy", "[image][dicom]")
{
  const std::vector<dicom::RawMetadataEntry> entries{
    {.tag = "0010|0010", .value = "Patient"},
    {.tag = "0019|100a", .value = "Private"},
    {.tag = "0008|0060", .value = "MR"}};

  const auto filtered = dicom::filteredMetadataEntries(entries);
  REQUIRE(filtered.size() == 1);
  CHECK(filtered.front().tag == "0008|0060");

  const auto withPrivate = dicom::filteredMetadataEntries(entries, true);
  REQUIRE(withPrivate.size() == 2);
  CHECK(withPrivate.at(0).tag == "0019|100a");
  CHECK(withPrivate.at(1).tag == "0008|0060");
}

TEST_CASE("DICOM middle-slice preview returns empty for series without files", "[image][dicom][preview]")
{
  dicom::SeriesInfo info;
  CHECK_FALSE(dicom::loadMiddleSlicePreview(info).has_value());
  CHECK(dicom::loadSlicePreviews(info).empty());

  dicom::SlicePreview preview;
  CHECK(preview.empty());
}

TEST_CASE("DICOM discovery reports missing and non-DICOM inputs without throwing", "[image][dicom][discovery]")
{
  const auto missing = dicom::discoverSeries({std::filesystem::temp_directory_path() / "entropy-missing-dicom-folder"});
  CHECK(missing.series.empty());
  REQUIRE_FALSE(missing.warnings.empty());

  const auto textFile = std::filesystem::temp_directory_path() / "entropy-not-dicom.txt";
  {
    std::ofstream out(textFile);
    out << "not a dicom file\n";
  }

  CHECK_FALSE(dicom::canReadDicomHeader(textFile));
  const auto result = dicom::discoverSeries({textFile});
  CHECK(result.series.empty());
}

TEST_CASE("DICOM loading rejects synthetic non-loadable and vector series records", "[image][dicom][loading]")
{
  dicom::SeriesInfo empty;
  CHECK_FALSE(dicom::loadSeriesImage(empty).has_value());

  dicom::SeriesInfo vectorSeries;
  vectorSeries.seriesInstanceUid = "1.2.3";
  vectorSeries.files.emplace_back("slice.dcm");
  vectorSeries.geometry.dimensions = {2, 2, 1};
  vectorSeries.geometry.spacing = {1.0f, 1.0f, 1.0f};
  CHECK_FALSE(dicom::loadSeriesImage(
                vectorSeries,
                Image::ImageRepresentation::Image,
                Image::MultiComponentBufferType::InterleavedImage)
                .has_value());
}

TEST_CASE("DICOM discovery preview and load work on an external fixture", "[image][dicom][integration]")
{
  const char* root = std::getenv("ENTROPY_DICOM_TEST_ROOT");
  if (!root || std::string(root).empty()) {
    return;
  }

  const auto result = dicom::discoverSeries({root});
  REQUIRE_FALSE(result.series.empty());

  const auto seriesIt = std::find_if(result.series.begin(), result.series.end(), [](const dicom::SeriesInfo& series) {
    return series.loadable();
  });
  REQUIRE(seriesIt != result.series.end());

  CHECK_FALSE(seriesIt->geometry.sliceOrientation.empty());

  const auto preview = dicom::loadMiddleSlicePreview(*seriesIt);
  REQUIRE(preview.has_value());
  CHECK_FALSE(preview->empty());
  CHECK(preview->width > 0);
  CHECK(preview->height > 0);

  const auto previews = dicom::loadSlicePreviews(*seriesIt, 128, 10);
  REQUIRE_FALSE(previews.empty());
  CHECK(previews.size() <= 10);
  CHECK(previews.front().width > 0);

  const auto image = dicom::loadSeriesImage(*seriesIt);
  REQUIRE(image.has_value());
  CHECK(image->hasPixelData());
  CHECK(image->header().pixelDimensions().x > 0);
  CHECK(image->header().pixelDimensions().y > 0);
  CHECK(image->header().pixelDimensions().z > 0);
}
