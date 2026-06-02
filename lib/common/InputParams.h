#pragma once

#include "common/Filesystem.h"

#include <spdlog/spdlog.h>

#include <optional>
#include <ostream>
#include <vector>

/// Path to an image and its corresponding segmentations
struct ImageSegPair
{
  fs::path image;
  std::vector<fs::path> segmentations;
};

/**
 * @brief Entropy input parameters read from command line
 */
struct InputParams
{
  /// All image and segmentation paths, where the first image is the reference image
  std::vector<ImageSegPair> imageFiles;

  /// An optional path to a project file that specifies images, segmentations,
  /// landmarks, and annotations in JSON format
  std::optional<fs::path> projectFile;

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
