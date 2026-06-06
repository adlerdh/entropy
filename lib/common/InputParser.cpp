#include "common/InputParser.h"
#include <spdlog/fmt/std.h>
#include "BuildStamp.h"

#undef max

#include <CLI/CLI.hpp>

// clang-format off
#include <spdlog/spdlog.h>
#include <spdlog/fmt/ostr.h>
// clang-format on

#include <algorithm> // std::equal
#include <cctype>    // std::tolower
#include <cstdlib>   // std::exit
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

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
  auto ichar_equals = [](char a, char b) -> bool {
    return (std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b)));
  };

  return std::equal(str1.begin(), str1.end(), str2.begin(), str2.end(), ichar_equals);
}

/**
 * @brief Validate the input parameters
 * @param[in,out] params Input parameters
 * @return True iff parameters are valid
 */
bool validateParams(InputParams& params)
{
  if (!params.projectFile && params.imageFiles.empty()) {
    spdlog::info("No image or project file provided; starting with an empty workspace");
    params.set = false;
    return true;
  }

  params.set = true;
  return true;
}

void assignConsoleLogLevel(const std::string& logLevel, InputParams& params)
{
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
}

void logInputs(const InputParams& params)
{
  if (!params.imageFiles.empty()) {
    spdlog::info("{} image(s) provided:", params.imageFiles.size());

    for (size_t i = 0; i < params.imageFiles.size(); ++i) {
      if (0 == i) {
        spdlog::info("\tImage[{}] (reference): {}", i, params.imageFiles[i].image);
      }
      else {
        spdlog::info("\tImage[{}]: {}", i, params.imageFiles[i].image);
      }

      if (params.imageFiles[i].segmentations.empty()) {
        spdlog::info("\tSegmentations for image[{}]: <none>", i);
      }
      else {
        for (size_t j = 0; j < params.imageFiles[i].segmentations.size(); ++j) {
          spdlog::info("\tSegmentation[{}][{}]: {}", i, j, params.imageFiles[i].segmentations[j]);
        }
      }
    }
  }
  else if (params.projectFile) {
    spdlog::info("Project file provided: {}", *params.projectFile);
  }
  else {
    spdlog::info("No image arguments or project file was provided");
  }
}

std::vector<char*> filterPlatformArguments(const int argc, char* argv[])
{
  std::vector<char*> filteredArgs;
  filteredArgs.reserve(static_cast<size_t>(argc));

  for (int i = 0; i < argc; ++i) {
    const std::string_view arg{argv[i]};
    if (arg.starts_with("-psn_")) {
      continue;
    }

    filteredArgs.push_back(argv[i]);
  }

  return filteredArgs;
}

} // namespace

bool parseCommandLine(const int argc, char* argv[], InputParams& params)
{
  params.set = false;
  params.imageFiles.clear();
  params.projectFile = std::nullopt;

  std::ostringstream desc;
  desc << APP_DESCRIPTION;

  CLI::App program{desc.str(), APP_NAME};
  program.set_version_flag("--version", VERSION_FULL);

  std::string logLevel = "info";
  program.add_option("-l,--log-level", logLevel, "console log level: {trace, debug, info, warn, err, critical, off}")
    ->default_val(logLevel);

  std::string projectFile;
  auto* projectOption = program.add_option("-p,--project", projectFile, "JSON project file");

  auto* imageOption = program
                        .add_option_function<std::string>(
                          "--image",
                          [&params](const std::string& imageFile) { params.imageFiles.push_back({imageFile, {}}); },
                          "image path; repeat for multiple images")
                        ->trigger_on_parse();

  auto* segOption = program
                      .add_option_function<std::vector<std::string> >(
                        "--seg",
                        [&params](const std::vector<std::string>& segFiles) {
                          if (params.imageFiles.empty()) {
                            throw CLI::ValidationError("--seg must follow an --image option");
                          }

                          auto& lastImage = params.imageFiles.back();
                          for (const std::string& segFile : segFiles) {
                            lastImage.segmentations.emplace_back(segFile);
                          }
                        },
                        "segmentation path for the preceding --image; repeat for multiple segmentations")
                      ->expected(1, -1)
                      ->trigger_on_parse();

  projectOption->excludes(imageOption)->excludes(segOption);
  imageOption->excludes(projectOption);
  segOption->excludes(projectOption);

  try {
    auto filteredArgs = filterPlatformArguments(argc, argv);
    program.parse(static_cast<int>(filteredArgs.size()), filteredArgs.data());
  }
  catch (const CLI::CallForHelp&) {
    std::cout << program.help();
    std::exit(EXIT_SUCCESS);
  }
  catch (const CLI::CallForVersion&) {
    std::cout << VERSION_FULL << '\n';
    std::exit(EXIT_SUCCESS);
  }
  catch (const CLI::ParseError& e) {
    spdlog::critical("Exception parsing arguments: {}", e.what());
    std::cout << program.help();
    return false;
  }

  if (!projectFile.empty()) {
    params.projectFile = projectFile;
  }

  assignConsoleLogLevel(logLevel, params);
  logInputs(params);

  // Final validation of parameters:
  if (validateParams(params)) {
    return true;
  }

  std::cout << program.help();
  return false;
}
