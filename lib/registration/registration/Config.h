#pragma once

#include "registration/Commands.h"
#include "registration/Types.h"

#include <filesystem>
#include <string>

namespace registration
{

/**
 * @brief Application-level settings for registration backends.
 */
struct BackendConfig
{
  Backend defaultBackend = Backend::Greedy;          //!< Backend selected for new jobs by default
  std::filesystem::path greedyExecutable = "greedy"; //!< Greedy command or executable path
  std::filesystem::path antsRegistrationExecutable = //!< ANTs antsRegistration executable path
    "antsRegistration";
  std::filesystem::path antsApplyTransformsExecutable = //!< ANTs antsApplyTransforms executable path
    "antsApplyTransforms";
  std::filesystem::path antsConvertTransformFileExecutable = //!< ANTs ConvertTransformFile executable path
    "ConvertTransformFile";
  std::filesystem::path fireAntsPythonExecutable = "python"; //!< Python executable used for FireANTs
  std::filesystem::path defaultOutputDirectory;              //!< Default registration output directory
  bool keepTemporaryFiles = false;                           //!< Preserve temporary exported inputs
  int maxConcurrentJobs = 1;                                 //!< Maximum external jobs running at once
  int defaultCpuThreadCount = 0;                             //!< Zero lets the backend choose
  std::string defaultFireAntsDevice = "cuda:0";              //!< Default PyTorch device for FireANTs
  bool showExpertOptionsByDefault = false;                   //!< Reveal expert controls in setup UI
};

/**
 * @brief Convert app/backend configuration to command-generation options.
 * @param config Backend configuration.
 * @return Command executable and bridge options.
 */
CommandGenerationOptions commandOptions(const BackendConfig& config);

/**
 * @brief Return the configured executable path for a backend.
 * @param config Backend configuration.
 * @param backend Backend to query.
 * @return Executable used to launch the backend.
 */
std::filesystem::path executableForBackend(const BackendConfig& config, Backend backend);

} // namespace registration
