#include "registration/Execution.h"

#include "registration/Progress.h"

namespace registration
{
namespace
{

void setStatus(JobExecution& execution, JobStatus status, const JobExecutionCallbacks& callbacks)
{
  execution.status = status;
  if (callbacks.onStatusChanged) {
    callbacks.onStatusChanged(status);
  }
}

void recordOutputLine(JobExecution& execution, const ProcessOutputLine& line, const JobExecutionCallbacks& callbacks)
{
  if (callbacks.onOutputLine) {
    callbacks.onOutputLine(line);
  }

  std::string error;
  const std::optional<ProgressEvent> event = parseProgressEventLine(line.text, &error);
  if (event) {
    execution.progressEvents.push_back(*event);
    if (event->kind == ProgressEventKind::Warning && !event->message.empty()) {
      execution.warnings.push_back(event->message);
    }
    if (callbacks.onProgressEvent) {
      callbacks.onProgressEvent(*event);
    }
  }
  else if (!error.empty() && line.stream == OutputStream::Stderr) {
    execution.warnings.push_back(line.text);
  }
}

std::string commandFailureMessage(const CommandSpec& command, const ProcessResult& result)
{
  if (result.launchFailed) {
    return result.failureMessage.empty() ? "Could not launch " + command.executable : result.failureMessage;
  }
  if (result.cancelled) {
    return "Registration was cancelled.";
  }
  if (!result.failureMessage.empty()) {
    return result.failureMessage;
  }
  return command.description + " failed with exit code " + std::to_string(result.exitCode) + ".";
}

} // namespace

JobExecution executeJob(const JobSpec& job, IProcessRunner& processRunner, const JobExecutionCallbacks& callbacks)
{
  JobExecution execution;
  std::vector<CommandSpec> commands = generateCommands(job);

  setStatus(execution, JobStatus::PreparingInputs, callbacks);
  if (commands.empty()) {
    execution.errorMessage = "No backend commands were generated.";
    setStatus(execution, JobStatus::Failed, callbacks);
    return execution;
  }

  ProcessOptions options;
  options.workingDirectory = job.outputDirectory;

  setStatus(execution, JobStatus::Running, callbacks);
  for (const CommandSpec& command : commands) {
    ProcessCallbacks processCallbacks;
    processCallbacks.onOutputLine = [&](const ProcessOutputLine& line) {
      recordOutputLine(execution, line, callbacks);
    };

    ProcessResult result = processRunner.run(command, options, processCallbacks);
    for (const ProcessOutputLine& line : result.outputLines) {
      recordOutputLine(execution, line, callbacks);
    }

    CommandExecution commandExecution;
    commandExecution.command = command;
    commandExecution.displayString = displayCommand(command);
    commandExecution.result = result;
    execution.commands.push_back(std::move(commandExecution));

    if (result.cancelled) {
      execution.errorMessage = commandFailureMessage(command, result);
      setStatus(execution, JobStatus::Cancelled, callbacks);
      return execution;
    }
    if (result.launchFailed || result.exitCode != 0) {
      execution.errorMessage = commandFailureMessage(command, result);
      setStatus(execution, JobStatus::Failed, callbacks);
      return execution;
    }
  }

  setStatus(execution, JobStatus::WritingOutputs, callbacks);
  setStatus(execution, JobStatus::Completed, callbacks);
  return execution;
}

} // namespace registration
