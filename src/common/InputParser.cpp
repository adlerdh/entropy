#include "common/InputParser.h"
#include "common/Filesystem.h"
#include "BuildStamp.h"

#undef max

#include <argparse/argparse.hpp>

// clang-format off
#include <spdlog/spdlog.h>
#include <spdlog/fmt/ostr.h>
// clang-format on

#include <algorithm> // std::equal
#include <cctype> // std::tolower
#include <iostream>
#include <optional>
#include <regex>
#include <sstream>

namespace
{

/**
 * @brief Check string case-insensitive equality
 * @param[in] str1 First string
 * @param[in] str2 Second string
 * @return True iff the strings are equal (case-insensitive)
 */
bool iequals(const std::string& str1, const std::string& str2)
{
  auto ichar_equals = [](char a, char b) -> bool
  {
    return (std::tolower(static_cast<unsigned char>(a)) ==
            std::tolower(static_cast<unsigned char>(b)));
  };

  return std::equal(str1.begin(), str1.end(), str2.begin(), str2.end(), ichar_equals);
}

/**
 * @brief Split string based on a delimiter character
 * @param[in] stringToSplit String to split
 * @param[in] delimiter Delimieter character
 * @return Vector of strings after split
 */
std::vector<std::string> splitStringByDelimiter(const std::string& stringToSplit, char delimiter)
{
  std::vector<std::string> splits;
  std::string split;
  std::istringstream ss(stringToSplit);

  while (std::getline(ss, split, delimiter)) {
    splits.push_back(split);
  }

  return splits;
}

/**
 * @brief Validate the input parameters
 * @param[in,out] params Inut parameters
 * @return True iff parameters are valid
 */
bool validateParams(InputParams& params)
{
  if (!params.projectFile && params.imageFiles.empty()) {
    spdlog::error("No image or project file provided");
    return false;
  }

  params.set = true;
  return true;
}

/**
 * @brief Parse a string containing a comma-separated pair of image and segmentation paths,
 * such as "imagePath.nii.gz,segPath.nii.gz". There is to be no space after the separating comma.
 *
 * @param[in] imgSegPairString String of comma-separated image and segmentation paths
 * @return Parsed image and segmentation paths
 */
ImageSegPair parseImageSegPair(const std::string& imgSegPairString)
{
  auto splitStrings = splitStringByDelimiter(imgSegPairString, ',');

  // Removing leading and trailing white space
  for (auto& s : splitStrings) {
    s = std::regex_replace(s, std::regex("^ +| +$|( ) +"), "$1");
  }

  ImageSegPair ret{.image = splitStrings[0], .seg = std::nullopt};

  if (splitStrings.size() > 1 && !splitStrings[1].empty()) {
    ret.seg = splitStrings[1]; // set segmentation, if present
  }

  return ret;
}

} // namespace

bool parseCommandLine(const int argc, char* argv[], InputParams& params)
{
  params.set = false;

  std::ostringstream desc;
  desc << APP_DESCRIPTION;

  argparse::ArgumentParser program(APP_NAME, VERSION_FULL);
  program.add_description(desc.str());

  program.add_argument("-l", "--log-level")
    .default_value(std::string("info"))
    .help("console log level: {trace, debug, info, warn, err, critical, off}");

  program.add_argument("-p", "--project").help("JSON project file");

  program.add_argument("images")
    .remaining() // so that a list of images can be provided
    .action(parseImageSegPair)
    .help("list of paths to images and optional segmentations: "
          "a corresponding image and segmentation pair is separated by a comma; "
          "images are separated by a space (e.g. img0[,seg0] img1 img2[,seg2] ...)");

  /// @note Remember to place all optional arguments BEFORE the remaining argument.
  /// If the optional argument is placed after the remaining arguments, it too will be deemed remaining

  try {
    program.parse_args(argc, argv);
  }
  catch (const std::exception& e) {
    spdlog::critical("Exception parsing arguments: {}", e.what());
    std::cout << program;
    return false;
  }

  // Get the inputs:
  std::string logLevel;

  try
  {
    const auto imageFiles = program.present<std::vector<ImageSegPair>>("images");
    const auto projectFile = program.present<std::string>("-p");

    if (imageFiles && projectFile) {
      spdlog::critical("Arguments for images and a project file were both provided. "
                       "Please specify either image arguments or a project file, but not both.");
      std::cout << program;
      return false;
    }

    if (imageFiles) {
      params.imageFiles = *imageFiles;
    }
    else if (projectFile) {
      params.projectFile = *projectFile;
    }

    logLevel = program.get<std::string>("-l");
  }
  catch (const std::exception& e) {
    spdlog::critical("Exception getting arguments: {}", e.what());
    std::cout << program;
    return false;
  }

  // Print out inputs after parsing:
  if (!params.imageFiles.empty())
  {
    spdlog::info("{} image(s) provided:", params.imageFiles.size());

    for (size_t i = 0; i < params.imageFiles.size(); ++i)
    {
      if (0 == i) {
        spdlog::info("\tImage[{}] (reference): {}", i, params.imageFiles[i].image);
      }
      else {
        spdlog::info("\tImage[{}]: {}", i, params.imageFiles[i].image);
      }

      spdlog::info("\tSegmentation for image[{}]: {}", i, params.imageFiles[i].seg.value_or("<none>"));
    }
  }
  else if (params.projectFile) {
    spdlog::info("Project file provided: {}", *params.projectFile);
  }
  else {
    spdlog::critical("No image arguments or project file was provided");
    std::cout << program;
    return false;
  }

  // Set the console log level:
  using enum spdlog::level::level_enum;

  if (iequals(logLevel, "trace")) {
    params.consoleLogLevel = trace;
  }
  else if (iequals(logLevel, "debug")) {
    params.consoleLogLevel = debug;
  }
  else if (iequals(logLevel, "info")) {
    params.consoleLogLevel = info;
  }
  else if (iequals(logLevel, "warn") || iequals(logLevel, "warning")) {
    params.consoleLogLevel = warn;
  }
  else if (iequals(logLevel, "err") || iequals(logLevel, "error")) {
    params.consoleLogLevel = err;
  }
  else if (iequals(logLevel, "critical")) {
    params.consoleLogLevel = critical;
  }
  else if (iequals(logLevel, "off")) {
    params.consoleLogLevel = off;
  }
  else {
    spdlog::warn("Invalid console log level: {}. Defaulting to info level.", logLevel);
    params.consoleLogLevel = info;
  }

  // Final validation of parameters:
  if (validateParams(params)) {
    return true;
  }

  std::cout << program;
  return false;
}
