#include "registration/Execution.h"

#include "registration/Artifacts.h"
#include "registration/Json.h"
#include "registration/Progress.h"

#include <nlohmann/json.hpp>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string_view>
#include <system_error>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#elif defined(__linux__)
#include <unistd.h>
#endif

namespace registration
{
namespace
{

void fillMissingExpectedArtifacts(const JobSpec& job, ResultManifest& manifest)
{
  const ResultManifest expectedManifest = buildExpectedResultManifest(job);
  if (manifest.warpedImage.empty()) {
    manifest.warpedImage = expectedManifest.warpedImage;
  }
  if (manifest.inverseWarp.empty()) {
    manifest.inverseWarp = expectedManifest.inverseWarp;
  }
  if (manifest.forwardWarp.empty()) {
    manifest.forwardWarp = expectedManifest.forwardWarp;
  }
  if (manifest.affineTransform.empty()) {
    manifest.affineTransform = expectedManifest.affineTransform;
  }
}

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

std::string pathListSeparator()
{
#ifdef _WIN32
  return ";";
#else
  return ":";
#endif
}

std::string environmentVariable(std::string_view name)
{
#ifdef _WIN32
  char* value = nullptr;
  std::size_t valueSize = 0;
  if (_dupenv_s(&value, &valueSize, std::string{name}.c_str()) != 0 || !value) {
    return {};
  }
  std::string result{value, valueSize > 0 ? valueSize - 1 : 0};
  std::free(value);
  return result;
#else
  if (const char* value = std::getenv(std::string{name}.c_str())) {
    return value;
  }
  return {};
#endif
}

std::string fireAntsBridgePythonPath()
{
#ifdef ENTROPY_FIREANTS_BRIDGE_PYTHONPATH
  return ENTROPY_FIREANTS_BRIDGE_PYTHONPATH;
#else
  return {};
#endif
}

std::filesystem::path executableDirectory()
{
#ifdef _WIN32
  std::vector<wchar_t> buffer(MAX_PATH);
  while (true) {
    const DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    if (length == 0) {
      return {};
    }
    if (length < buffer.size() - 1) {
      return std::filesystem::path{buffer.data()}.parent_path();
    }
    buffer.resize(buffer.size() * 2);
  }
#elif defined(__APPLE__)
  std::vector<char> buffer(1024);
  while (true) {
    uint32_t size = static_cast<uint32_t>(buffer.size());
    if (_NSGetExecutablePath(buffer.data(), &size) == 0) {
      return std::filesystem::weakly_canonical(std::filesystem::path{buffer.data()}).parent_path();
    }
    buffer.resize(size);
  }
#elif defined(__linux__)
  std::vector<char> buffer(1024);
  while (true) {
    const ssize_t length = readlink("/proc/self/exe", buffer.data(), buffer.size());
    if (length < 0) {
      return {};
    }
    if (static_cast<std::size_t>(length) < buffer.size()) {
      return std::filesystem::path{std::string{buffer.data(), static_cast<std::size_t>(length)}}.parent_path();
    }
    buffer.resize(buffer.size() * 2);
  }
#else
  return {};
#endif
}

std::vector<std::filesystem::path> fireAntsBridgePythonPaths()
{
  std::vector<std::filesystem::path> paths;
  if (const std::string sourcePath = fireAntsBridgePythonPath(); !sourcePath.empty()) {
    paths.emplace_back(sourcePath);
  }

  const std::filesystem::path exeDir = executableDirectory();
  if (!exeDir.empty()) {
#ifdef _WIN32
    paths.push_back(exeDir / "share" / "entropy" / "python");
#elif defined(__APPLE__)
    paths.push_back((exeDir / ".." / "Resources" / "python").lexically_normal());
#elif defined(__linux__)
    paths.push_back((exeDir / ".." / "share" / "entropy" / "python").lexically_normal());
#endif
  }

  return paths;
}

void configureFireAntsEnvironment(const JobSpec& job, ProcessOptions& options)
{
  if (job.backend != Backend::FireANTs) {
    return;
  }

  std::string pythonPath;
  for (const std::filesystem::path& path : fireAntsBridgePythonPaths()) {
    if (path.empty()) {
      continue;
    }
    if (!pythonPath.empty()) {
      pythonPath += pathListSeparator();
    }
    pythonPath += path.string();
  }

  if (const std::string existingPythonPath = environmentVariable("PYTHONPATH"); !existingPythonPath.empty()) {
    if (!pythonPath.empty()) {
      pythonPath += pathListSeparator();
    }
    pythonPath += existingPythonPath;
  }

  if (!pythonPath.empty()) {
    options.environment.push_back(EnvironmentVariable{"PYTHONPATH", std::move(pythonPath)});
  }
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
  configureFireAntsEnvironment(job, options);

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
    processCallbacks.shouldCancel = callbacks.shouldCancel;

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
  else {
    fillMissingExpectedArtifacts(job, *execution.manifest);
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
