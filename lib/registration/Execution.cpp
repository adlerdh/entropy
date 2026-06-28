#include "registration/Execution.h"

#include "registration/Artifacts.h"
#include "registration/Json.h"
#include "registration/Progress.h"

#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>
#include <system_error>

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

bool isCancellationRequested(const JobExecutionCallbacks& callbacks)
{
  return callbacks.shouldCancel && callbacks.shouldCancel();
}

void recordOutputLine(JobExecution& execution, const ProcessOutputLine& line, const JobExecutionCallbacks& callbacks)
{
  execution.outputLines.push_back(line);

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

bool prepareOutputDirectory(const JobSpec& job, JobExecution& execution)
{
  if (job.outputDirectory.empty()) {
    execution.errorMessage = "Registration output directory is empty.";
    return false;
  }

  std::error_code error;
  std::filesystem::create_directories(job.outputDirectory, error);
  if (error) {
    execution.errorMessage = "Could not create registration output directory: " + error.message();
    return false;
  }

  return true;
}

bool writeFireAntsJobSpec(const JobSpec& job, JobExecution& execution)
{
  if (job.backend != Backend::FireANTs) {
    return true;
  }

  const std::filesystem::path path = artifactPath(job, ArtifactRole::JobSpec);
  std::ofstream stream(path);
  if (!stream) {
    execution.errorMessage = "Could not write FireANTs job specification: " + path.string();
    return false;
  }

  const nlohmann::json json = job;
  stream << json.dump(2) << '\n';
  if (!stream) {
    execution.errorMessage = "Could not finish writing FireANTs job specification: " + path.string();
    return false;
  }

  return true;
}

std::optional<ResultManifest> readBackendResultManifest(const JobSpec& job, std::string& warning)
{
  const std::filesystem::path path = artifactPath(job, ArtifactRole::ResultManifest);
  if (!std::filesystem::exists(path)) {
    return std::nullopt;
  }

  std::ifstream stream(path);
  if (!stream) {
    warning = "Could not read registration result manifest: " + path.string();
    return std::nullopt;
  }

  try {
    nlohmann::json json;
    stream >> json;
    return json.get<ResultManifest>();
  }
  catch (const std::exception& e) {
    warning = "Could not parse registration result manifest " + path.string() + ": " + e.what();
    return std::nullopt;
  }
}

} // namespace

JobExecution executeJob(
  const JobSpec& job,
  const CommandGenerationOptions& commandOptions,
  IProcessRunner& processRunner,
  const JobExecutionCallbacks& callbacks)
{
  JobExecution execution;
  std::vector<CommandSpec> commands = generateCommands(job, commandOptions);

  setStatus(execution, JobStatus::PreparingInputs, callbacks);
  if (commands.empty()) {
    execution.errorMessage = "No backend commands were generated.";
    setStatus(execution, JobStatus::Failed, callbacks);
    return execution;
  }

  ProcessOptions options;
  options.workingDirectory = job.outputDirectory;

  if (!prepareOutputDirectory(job, execution) || !writeFireAntsJobSpec(job, execution)) {
    setStatus(execution, JobStatus::Failed, callbacks);
    return execution;
  }

  setStatus(execution, JobStatus::Running, callbacks);
  for (const CommandSpec& command : commands) {
    if (isCancellationRequested(callbacks)) {
      execution.errorMessage = "Registration was cancelled.";
      setStatus(execution, JobStatus::Cancelled, callbacks);
      return execution;
    }

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
    if (isCancellationRequested(callbacks)) {
      execution.errorMessage = "Registration was cancelled.";
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
  std::string manifestWarning;
  execution.manifest = readBackendResultManifest(job, manifestWarning);
  if (!manifestWarning.empty()) {
    execution.warnings.push_back(manifestWarning);
  }
  if (!execution.manifest) {
    execution.manifest = buildExpectedResultManifest(job);
  }
  execution.manifest->success = true;
  setStatus(execution, JobStatus::Completed, callbacks);
  return execution;
}

JobExecution executeJob(const JobSpec& job, IProcessRunner& processRunner, const JobExecutionCallbacks& callbacks)
{
  return executeJob(job, CommandGenerationOptions{}, processRunner, callbacks);
}

} // namespace registration
