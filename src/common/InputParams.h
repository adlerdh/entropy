#ifndef INPUT_PARAMS_H
#define INPUT_PARAMS_H

#include <spdlog/spdlog.h>

#include <optional>
#include <ostream>
#include <string>
#include <utility>

/**
 * @brief Entropy input parameters read from command line
 */
struct InputParams
{
  /// Path to an image and, optionally, its corresponding segmentation.
  using ImageSegPair = std::pair<std::string, std::optional<std::string> >;

  /// All image and segmentation paths. The first image is the reference image.
  std::vector<ImageSegPair> imageFiles;

  /// An optional path to a project file that specifies images, segmentations,
  /// landmarks, and annotations in JSON format.
  std::optional<std::string> projectFile;

  /// Console logging level
  spdlog::level::level_enum consoleLogLevel;

  /// Flag indicating that the parameters been successfully set
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

#endif // INPUT_PARAMS_H
