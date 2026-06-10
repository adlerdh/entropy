#pragma once

#include "image/Image.h"

#include <glm/mat3x3.hpp>
#include <glm/vec3.hpp>
#include <glm/geometric.hpp>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace dicom
{
/**
 * @brief Raw DICOM metadata key/value pair before privacy filtering and display-name assignment.
 */
struct RawMetadataEntry
{
  std::string tag;
  std::string value;
};

/**
 * @brief Privacy-filtered DICOM metadata value for display in the series-selection UI.
 */
struct MetadataEntry
{
  std::string tag;
  std::string name;
  std::string value;
};

/**
 * @brief Geometry read from a DICOM series before loading the image volume.
 */
struct SeriesGeometry
{
  glm::uvec3 dimensions{0, 0, 0};
  glm::vec3 spacing{0.0f, 0.0f, 0.0f};
  glm::vec3 origin{0.0f, 0.0f, 0.0f};
  glm::mat3 directions{1.0f};
  std::string sliceOrientation;
};

struct SeriesMetadata
{
  std::string studyInstanceUid;
  std::string seriesInstanceUid;
  std::string studyDescription;
  std::string seriesDescription;
  std::string protocolName;
  std::string modality;
  std::string manufacturer;
  std::string seriesNumber;
  std::string acquisitionDate;
};

/**
 * @brief Small grayscale preview of one representative DICOM slice.
 */
struct SlicePreview
{
  std::uint32_t width = 0;
  std::uint32_t height = 0;
  std::size_t sliceIndex = 0;
  std::filesystem::path fileName;
  std::vector<std::uint8_t> pixels;

  /**
   * @brief Check whether the preview has valid dimensions and pixel data.
   * @return True when the preview is empty.
   */
  bool empty() const;
};

/**
 * @brief Discovered DICOM series with loadability, geometry, and display metadata.
 */
struct SeriesInfo
{
  std::filesystem::path rootPath;
  std::string seriesInstanceUid;
  std::string displayName;
  std::vector<std::filesystem::path> files;
  SeriesGeometry geometry;
  SeriesMetadata metadata;
  std::vector<MetadataEntry> metadataSummary;
  std::vector<std::string> warnings;

  /**
   * @brief Return whether this series is ready to load without known warnings.
   * @return True when the series has files and no warnings.
   */
  bool loadable() const;
};

/**
 * @brief Options controlling DICOM discovery and metadata exposure.
 */
struct DiscoverOptions
{
  bool recursive = true;
  bool includePrivateMetadata = false;
};

struct DiscoverResult
{
  std::vector<SeriesInfo> series;
  std::vector<std::string> warnings;
};

/**
 * @brief Check whether a file can be read as a DICOM image header.
 * @param fileName File to inspect.
 * @return True if GDCM can read the file header.
 */
bool canReadDicomHeader(const std::filesystem::path& fileName);

/**
 * @brief Return a human-readable DICOM data element name for a tag.
 * @param tag DICOM tag in `gggg|eeee` form.
 * @return Known tag name, or `Unknown` when the tag is not in the local dictionary.
 */
std::string metadataTagName(const std::string& tag);

/**
 * @brief Check whether a tag is excluded as patient-identifying information.
 * @param tag DICOM tag in `gggg|eeee` form.
 * @return True when the tag is treated as PHI.
 */
bool isPhiTag(const std::string& tag);
/**
 * @brief Check whether a tag belongs to a private DICOM group.
 * @param tag DICOM tag in `gggg|eeee` form.
 * @return True when the tag group is private.
 */
bool isPrivateTag(const std::string& tag);

/**
 * @brief Check whether metadata should be exposed in the UI.
 * @param tag DICOM tag in `gggg|eeee` form.
 * @param includePrivateMetadata Whether to include private tags.
 * @return True when the tag passes the privacy filter.
 */
bool includeMetadataTag(const std::string& tag, bool includePrivateMetadata = false);

/**
 * @brief Convert raw metadata pairs into privacy-filtered display metadata.
 * @param entries Raw DICOM metadata entries.
 * @param includePrivateMetadata Whether to include private tags.
 * @return De-duplicated metadata entries with readable tag names.
 */
std::vector<MetadataEntry> filteredMetadataEntries(
  const std::vector<RawMetadataEntry>& entries,
  bool includePrivateMetadata = false);
/**
 * @brief Build a user-facing series name from DICOM metadata.
 * @param metadata DICOM series metadata.
 * @param seriesInstanceUid Fallback series UID.
 * @return Stable display name.
 */
std::string displayNameForSeries(const SeriesMetadata& metadata, const std::string& seriesInstanceUid);

/**
 * @brief Discover DICOM image series under folders or parent folders of files.
 * @param inputPaths Folders or DICOM files to scan.
 * @param options Discovery options.
 * @return Discovered series and scan warnings.
 * @throws Does not intentionally throw; unexpected exceptions are converted to warnings.
 */
DiscoverResult discoverSeries(
  const std::vector<std::filesystem::path>& inputPaths,
  const DiscoverOptions& options = {});
/**
 * @brief Load a discovered DICOM series as an Entropy image.
 * @param series Series returned from `discoverSeries`.
 * @param imageRep Image representation to load.
 * @param bufferType Multi-component buffer mode.
 * @return Loaded image, or `std::nullopt` if loading failed.
 * @throws Does not intentionally throw; load exceptions are logged and converted to `std::nullopt`.
 */
std::optional<Image> loadSeriesImage(
  const SeriesInfo& series,
  Image::ImageRepresentation imageRep = Image::ImageRepresentation::Image,
  Image::MultiComponentBufferType bufferType = Image::MultiComponentBufferType::SeparateImages);

/**
 * @brief Load downsampled grayscale previews spaced evenly along a DICOM series.
 * @param series Series returned from `discoverSeries`.
 * @param maxDimension Maximum width or height of each returned preview in pixels.
 * @param maxSlices Maximum number of preview slices to load.
 * @return Preview images, or an empty vector when preview loading failed.
 * @throws Does not intentionally throw; load exceptions are logged and converted to an empty vector.
 */
std::vector<SlicePreview>
loadSlicePreviews(const SeriesInfo& series, std::uint32_t maxDimension = 128, std::size_t maxSlices = 10);

/**
 * @brief Load a downsampled grayscale preview of the middle slice in a DICOM series.
 * @param series Series returned from `discoverSeries`.
 * @param maxDimension Maximum width or height of the returned preview in pixels.
 * @return Preview image, or `std::nullopt` when preview loading failed.
 * @throws Does not intentionally throw; load exceptions are logged and converted to `std::nullopt`.
 */
std::optional<SlicePreview> loadMiddleSlicePreview(const SeriesInfo& series, std::uint32_t maxDimension = 128);
} // namespace dicom
