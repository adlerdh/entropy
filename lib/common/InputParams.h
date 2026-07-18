#pragma once

#include <spdlog/spdlog.h>

#include <filesystem>
#include <optional>
#include <ostream>
#include <vector>

/// Path to an image and its corresponding segmentations
struct ImageSegPair
{
  std::filesystem::path image;
  std::vector<std::filesystem::path> segmentations;
};

/**
 * @brief Entropy input parameters read from command line
 */
struct InputParams
{
  /// All image and segmentation paths, where the first image is the reference image
  std::vector<ImageSegPair> imageFiles;

  /// DICOM folders or files to scan for loadable image series
  std::vector<std::filesystem::path> dicomPaths;

  /// An optional path to a project file that specifies images, segmentations,
  /// landmarks, and annotations in JSON format
  std::optional<std::filesystem::path> projectFile;

  /// Optional standalone layout JSON file that overrides generated/project layouts
  std::optional<std::filesystem::path> layoutsFile;

  /// Console logging level
  spdlog::level::level_enum consoleLogLevel;

  /// Flag indicating that the parameters have been successfully set
  bool set = false;
};

std::ostream& operator<<(std::ostream&, const InputParams&);

#include <spdlog/fmt/ostr.h>
#if FMT_VERSION >= 90000
template<>
struct fmt::formatter<InputParams> : ostream_formatter
{
};
#endif
