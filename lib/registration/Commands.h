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
 * @brief Generate backend process commands for preview and execution.
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
