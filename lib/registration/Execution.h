#pragma once

#include "registration/Commands.h"
#include "registration/Process.h"
#include "registration/Types.h"

#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace registration
{

/**
 * @brief Summary of one backend command execution.
 */
struct CommandExecution
{
  CommandSpec command;       //!< Command that was run.
  ProcessResult result;      //!< Process result.
  std::string displayString; //!< User-facing command preview string.
};

/**
 * @brief Completed registration job execution summary.
 */
struct JobExecution
{
  JobStatus status = JobStatus::Queued;       //!< Final normalized status.
  std::vector<CommandExecution> commands;     //!< Command results in execution order.
  std::vector<ProgressEvent> progressEvents;  //!< Parsed structured progress events.
  std::vector<ProcessOutputLine> outputLines; //!< Raw backend stdout/stderr lines.
  std::vector<std::string> warnings;          //!< Non-fatal warnings.
  std::optional<ResultManifest> manifest;     //!< Expected or backend-written result manifest.
  std::string errorMessage;                   //!< Fatal error summary.
};

/**
 * @brief Callbacks invoked while a job runs.
 */
struct JobExecutionCallbacks
{
  std::function<void(JobStatus)> onStatusChanged;             //!< Called when the normalized status changes.
  std::function<void(const ProgressEvent&)> onProgressEvent;  //!< Called for every parsed progress event.
  std::function<void(const ProcessOutputLine&)> onOutputLine; //!< Called for raw stdout/stderr lines.
  std::function<bool()> shouldCancel;                         //!< Return true when execution should stop.
};

/**
 * @brief Run a generated registration command sequence.
 * @param job Validated job specification.
 * @param commandOptions Executable and bridge options.
 * @param processRunner Process runner implementation.
 * @param callbacks Optional job callbacks.
 * @return Final job execution summary.
 */
JobExecution executeJob(
  const JobSpec& job,
  const CommandGenerationOptions& commandOptions,
  IProcessRunner& processRunner,
  const JobExecutionCallbacks& callbacks = {});

/**
 * @brief Run a generated registration command sequence using default executable names.
 * @param job Validated job specification.
 * @param processRunner Process runner implementation.
 * @param callbacks Optional job callbacks.
 * @return Final job execution summary.
 */
JobExecution executeJob(const JobSpec& job, IProcessRunner& processRunner, const JobExecutionCallbacks& callbacks = {});

} // namespace registration
