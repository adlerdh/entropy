#pragma once

#include "registration/Types.h"

#include <string>
#include <vector>

namespace registration
{

/**
 * @brief One external process invocation in a registration workflow.
 */
struct CommandSpec
{
  std::string description;       //!< User-facing step description.
  std::string executable;        //!< Executable or interpreter path.
  std::vector<std::string> args; //!< Arguments passed without shell interpretation.
};

/**
 * @brief External executable names and bridge module used when generating backend commands.
 */
struct CommandGenerationOptions
{
  std::string greedyExecutable = "greedy";                      //!< Greedy command or executable path.
  std::string antsRegistrationExecutable = "antsRegistration";  //!< ANTs registration executable path.
  std::string fireAntsPythonExecutable = "python";              //!< Python executable used for FireANTs.
  std::string fireAntsBridgeModule = "entropy_fireants_bridge"; //!< Python bridge module name.
};

/**
 * @brief Generate backend process commands for preview and execution.
 * @param job Validated registration job.
 * @param options Executable and bridge options.
 * @return Ordered commands needed to produce requested outputs.
 */
std::vector<CommandSpec> generateCommands(const JobSpec& job, const CommandGenerationOptions& options);

/**
 * @brief Generate backend process commands using default executable names.
 * @param job Validated registration job.
 * @return Ordered commands needed to produce requested outputs.
 */
std::vector<CommandSpec> generateCommands(const JobSpec& job);

/**
 * @brief Convert a command to a display string for expert preview.
 * @param command Command to format.
 * @return Shell-like display string. It is for display only, not shell execution.
 */
std::string displayCommand(const CommandSpec& command);

} // namespace registration
