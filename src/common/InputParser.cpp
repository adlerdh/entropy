#include "common/InputParser.h"
#include "defines.h"

#undef max

#include <argparse/argparse.hpp>
#include <spdlog/spdlog.h>

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
 * @param[in] a First string
 * @param[in] b Second string
 * @return True iff the strings are equal (case-insensitive)
 */
bool iequals(const std::string& a, const std::string& b)
{
  auto ichar_equals = [](char a, char b) -> bool
  {
    return std::tolower(static_cast<unsigned char>(a))
           == std::tolower(static_cast<unsigned char>(b));
  };

  return std::equal(a.begin(), a.end(), b.begin(), b.end(), ichar_equals);
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

  while (std::getline(ss, split, delimiter))
  {
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
  if (!params.projectFile && params.imageFiles.empty())
  {
    spdlog::critical("No image or project file provided");
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
InputParams::ImageSegPair parseImageSegPair(const std::string& imgSegPairString)
{
  auto splitStrings = splitStringByDelimiter(imgSegPairString, ',');
  for (auto& x : splitStrings)
  {
    // Removing leading and trailing white space
    x = std::regex_replace(x, std::regex("^ +| +$|( ) +"), "$1");
  }

  InputParams::ImageSegPair ret{splitStrings[0], std::nullopt};

  if (splitStrings.size() > 1 && !splitStrings[1].empty())
  {
    ret.second = splitStrings[1]; // set segmentation, if present
  }

  return ret;
}

} // namespace

int parseCommandLine(const int argc, char* argv[], InputParams& params)
{
  params.set = false;

  std::ostringstream desc;
  desc << "3D image differencing tool (" << ENTROPY_ORGNAME_LINE1 << ", " << ENTROPY_ORGNAME_LINE2
       << ", " << ENTROPY_ORGNAME_LINE3 << ")";

  argparse::ArgumentParser program(ENTROPY_APPNAME_FULL, ENTROPY_VERSION_FULL);

  program.add_description(desc.str());

  program.add_argument("-l", "--log-level")
    .default_value(std::string("info"))
    .help("console log level: {trace, debug, info, warn, err, critical, off}");

  program.add_argument("-p", "--project").help("project file in JSON format");

  program.add_argument("images")
    .remaining() // so that a list of images can be provided
    .action(parseImageSegPair)
    .help("list of paths to images and optional segmentations: "
          "a corresponding image and segmentation pair is separated by a comma "
          "and images are separated by a space (i.e. img0[,seg0] img1 img2[,seg2] ...)");

  /// @note Remember to place all optional arguments BEFORE the remaining argument.
  /// If the optional argument is placed after the remaining arguments, it too will be deemed remaining

  try
  {
    program.parse_args(argc, argv);
  }
  catch (const std::exception& e)
  {
    spdlog::critical("Exception parsing arguments: {}", e.what());
    std::cout << program;
    return EXIT_FAILURE;
  }

  // Get the inputs:
  std::string logLevel;

  try
  {
    const std::optional<std::vector<InputParams::ImageSegPair> > imageFiles
      = program.present<std::vector<InputParams::ImageSegPair> >("images");

    const std::optional<std::string> projectFile = program.present<std::string>("-p");

    if (imageFiles && projectFile)
    {
      spdlog::critical("Arguments for images and a project file were BOTH provided. "
                       "Please specify EITHER image arguments or a project file, but not both.");
      std::cout << program;
      return EXIT_FAILURE;
    }

    if (imageFiles)
    {
      params.imageFiles = *imageFiles;
    }
    else if (projectFile)
    {
      params.projectFile = *projectFile;
    }

    logLevel = program.get<std::string>("-l");
  }
  catch (const std::exception& e)
  {
    spdlog::critical("Exception getting arguments: {}", e.what());
    std::cout << program;
    return EXIT_FAILURE;
  }

  // Print out inputs after parsing:

  if (!params.imageFiles.empty())
  {
    spdlog::info("{} image(s) provided:", params.imageFiles.size());

    for (size_t i = 0; i < params.imageFiles.size(); ++i)
    {
      const auto& imgSegPair = params.imageFiles[i];

      if (0 == i)
      {
        spdlog::info("\tImage {} (reference): {}", i, imgSegPair.first);
      }
      else
      {
        spdlog::info("\tImage {}: {}", i, imgSegPair.first);
      }

      spdlog::info(
        "\tSegmentation for image {}: {}", i, (imgSegPair.second ? *imgSegPair.second : "<none>")
      );
    }
  }
  else if (params.projectFile)
  {
    spdlog::info("Project file provided: {}", *params.projectFile);
  }
  else
  {
    spdlog::critical("No image arguments or project file was provided");
    std::cout << program;
    return EXIT_FAILURE;
  }

  // Set the console log level:
  if (iequals(logLevel, "trace"))
  {
    params.consoleLogLevel = spdlog::level::level_enum::trace;
  }
  else if (iequals(logLevel, "debug"))
  {
    params.consoleLogLevel = spdlog::level::level_enum::debug;
  }
  else if (iequals(logLevel, "info"))
  {
    params.consoleLogLevel = spdlog::level::level_enum::info;
  }
  else if (iequals(logLevel, "warn") || iequals(logLevel, "warning"))
  {
    params.consoleLogLevel = spdlog::level::level_enum::warn;
  }
  else if (iequals(logLevel, "err") || iequals(logLevel, "error"))
  {
    params.consoleLogLevel = spdlog::level::level_enum::err;
  }
  else if (iequals(logLevel, "critical"))
  {
    params.consoleLogLevel = spdlog::level::level_enum::critical;
  }
  else if (iequals(logLevel, "off"))
  {
    params.consoleLogLevel = spdlog::level::level_enum::off;
  }
  else
  {
    spdlog::error("Invalid console log level: {}", logLevel);
    std::cout << program;
    return EXIT_FAILURE;
  }

  // Final validation of parameters:
  if (validateParams(params))
  {
    return EXIT_SUCCESS;
  }

  std::cout << program;
  return EXIT_FAILURE;
}
