#include "ui/ImGuiWrapper.h"

#include "common/UuidUtility.h"
#include "logic/app/Data.h"
#include "logic/app/RegistrationInputs.h"
#include "registration/Availability.h"
#include "registration/Config.h"
#include "registration/Process.h"
#include "registration/Scheduling.h"

#include <spdlog/fmt/std.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <chrono>
#include <functional>
#include <iterator>
#include <system_error>
#include <utility>

namespace fs = std::filesystem;

namespace
{
class ProcessExecutableProbe final : public registration::IExecutableProbe
{
public:
  explicit ProcessExecutableProbe(registration::IProcessRunner& runner) : m_runner(runner) {}

  registration::ExecutableProbeResult probe(
    const std::filesystem::path& executable,
    const std::vector<std::string>& arguments) override
  {
    registration::CommandSpec command;
    command.executable = executable.string();
    command.args = arguments;
    command.description = "Registration backend compatibility check";

    registration::ProcessOptions options;
    options.mergeStdErrIntoStdOut = true;
    const registration::ProcessResult processResult = m_runner.run(command, options, {});

    registration::ExecutableProbeResult result;
    result.found = !processResult.launchFailed && processResult.exitCode != 127;
    result.exitCode = processResult.exitCode;
    result.failureMessage = processResult.failureMessage;
    for (const registration::ProcessOutputLine& line : processResult.outputLines) {
      if (line.stream == registration::OutputStream::Stderr) {
        result.standardError += line.text;
        result.standardError += '\n';
      }
      else {
        result.standardOutput += line.text;
        result.standardOutput += '\n';
      }
    }
    return result;
  }

private:
  registration::IProcessRunner& m_runner;
};

bool pathIsWithinDirectory(const fs::path& path, const fs::path& directory)
{
  std::error_code error;
  const fs::path normalizedPath = fs::weakly_canonical(path, error);
  if (error) {
    return false;
  }

  const fs::path normalizedDirectory = fs::weakly_canonical(directory, error);
  if (error) {
    return false;
  }

  auto pathIt = normalizedPath.begin();
  for (auto dirIt = normalizedDirectory.begin(); dirIt != normalizedDirectory.end(); ++dirIt, ++pathIt) {
    if (pathIt == normalizedPath.end() || *pathIt != *dirIt) {
      return false;
    }
  }
  return true;
}

void removeTemporaryRegistrationInputs(const registration::JobSpec& job, registration::JobExecution& execution)
{
  if (job.outputDirectory.empty()) {
    return;
  }

  std::error_code error;
  const fs::path tempDirectory = fs::temp_directory_path(error);
  if (error || !pathIsWithinDirectory(job.outputDirectory, tempDirectory)) {
    return;
  }

  for (const auto& path : job.ownedInputFiles) {
    if (path.empty() || !pathIsWithinDirectory(path, job.outputDirectory)) {
      continue;
    }

    error.clear();
    if (fs::is_regular_file(path, error) && !error) {
      error.clear();
      (void)fs::remove(path, error);
      if (error) {
        execution.warnings.push_back(
          "Could not remove temporary registration input " + path.string() + ": " + error.message());
      }
    }
  }
}

bool checkRegistrationBackendBeforeLaunch(
  const registration::JobSpec& job,
  const registration::BackendConfig& config,
  registration::IProcessRunner& runner,
  registration::JobExecution& execution)
{
  if (job.backend == registration::Backend::FireANTs) {
    return true;
  }

  ProcessExecutableProbe probe{runner};
  const registration::BackendAvailability availability =
    registration::checkBackendAvailability(job.backend, config, probe);
  if (availability.status != registration::BackendAvailabilityStatus::Available) {
    execution.status = registration::JobStatus::Failed;
    execution.errorMessage = availability.message;
    return false;
  }

  if (availability.compatibility != registration::BackendCompatibilityStatus::Compatible) {
    execution.warnings.push_back(availability.compatibilityMessage);
  }
  return true;
}

} // namespace

bool ImGuiWrapper::materializeRegistrationInputs(registration::JobSpec& job)
{
  return registration_inputs::materialize(m_appData, job);
}

void ImGuiWrapper::requestQueuedRegistrationJobs()
{
  const int maxConcurrentJobs = std::max(1, m_appData.settings().registrationBackendConfig().maxConcurrentJobs);
  std::size_t runningJobs = 0;
  {
    std::lock_guard<std::mutex> lock(m_registrationJobFuturesMutex);
    runningJobs = m_runningRegistrationJobIds.size();
  }

  const registration::BackendConfig config = m_appData.settings().registrationBackendConfig();
  const registration::CommandGenerationOptions commandOptions = registration::commandOptions(config);
  const auto postEmptyEvent = m_postEmptyGlfwEvent;

  std::vector<std::pair<std::string, registration::JobSpec>> jobsToLaunch;
  registration::dispatchQueuedJobs(
    m_appData.registrationJobs().jobs(),
    runningJobs,
    static_cast<std::size_t>(maxConcurrentJobs),
    [this](const std::string& id) {
      std::lock_guard<std::mutex> lock(m_registrationJobFuturesMutex);
      return m_runningRegistrationJobIds.contains(id);
    },
    [this](const std::string& id) {
      std::lock_guard<std::mutex> lock(m_registrationJobFuturesMutex);
      if (const auto it = m_registrationJobCancelFlags.find(id); it != m_registrationJobCancelFlags.end())
        it->second->store(true);
    },
    [this, &jobsToLaunch](const registration::JobRecord& job) {
      registration::JobSpec jobSpec = job.spec;
      if (!materializeRegistrationInputs(jobSpec)) {
        registration::ProgressEvent event;
        event.kind = registration::ProgressEventKind::Failed;
        event.message = "Unable to export registration inputs for backend execution.";
        m_appData.registrationJobs().appendProgress(job.id, std::move(event));
        return false;
      }
      jobsToLaunch.emplace_back(job.id, std::move(jobSpec));
      return true;
    });

  for (const auto& [jobId, jobSpec] : jobsToLaunch) {
    if (auto* record = m_appData.registrationJobs().find(jobId)) {
      // Keep original input paths for project provenance; backend snapshots may be cleaned up.
      // The generated initial affine affects ANTs artifact numbering during output import.
      record->spec.initialAffineTransform = jobSpec.initialAffineTransform;
    }
    m_appData.registrationJobs().setStatus(jobId, registration::JobStatus::Running);
    const uuids::uuid taskUid = generateRandomUuid();
    auto cancelFlag = std::make_shared<std::atomic_bool>(false);
    const bool keepTemporaryFiles = config.keepTemporaryFiles;
    auto* pendingOutputLines = &m_pendingRegistrationOutputLines;
    auto* registrationMutex = &m_registrationJobFuturesMutex;
    auto future = std::async(
      std::launch::async,
      [jobId,
       jobSpec,
       config,
       commandOptions,
       postEmptyEvent,
       cancelFlag,
       keepTemporaryFiles,
       pendingOutputLines,
       registrationMutex]() {
        registration::JobExecution execution;
        try {
          registration::ShellProcessRunner runner;
          registration::JobExecutionCallbacks callbacks;
          callbacks.shouldCancel = [cancelFlag]() {
            return cancelFlag->load();
          };
          callbacks.onOutputLine = [jobId, postEmptyEvent, pendingOutputLines, registrationMutex](
                                     const registration::ProcessOutputLine& line) {
            {
              std::lock_guard<std::mutex> lock(*registrationMutex);
              pendingOutputLines->push_back(PendingRegistrationOutputLine{jobId, line});
            }
            if (postEmptyEvent) {
              postEmptyEvent();
            }
          };
          if (checkRegistrationBackendBeforeLaunch(jobSpec, config, runner, execution)) {
            std::vector<std::string> preflightWarnings = std::move(execution.warnings);
            execution = registration::executeJob(jobSpec, commandOptions, runner, callbacks);
            execution.warnings.insert(
              execution.warnings.begin(),
              std::make_move_iterator(preflightWarnings.begin()),
              std::make_move_iterator(preflightWarnings.end()));
          }
          execution.outputLines.clear();
          if (!keepTemporaryFiles && execution.status != registration::JobStatus::Failed) {
            removeTemporaryRegistrationInputs(jobSpec, execution);
          }
        }
        catch (const std::exception& e) {
          execution.status = registration::JobStatus::Failed;
          execution.errorMessage = e.what();
        }

        if (postEmptyEvent) {
          postEmptyEvent();
        }
        return RegistrationJobTaskResult{jobId, std::move(execution)};
      });

    {
      std::lock_guard<std::mutex> lock(m_registrationJobFuturesMutex);
      m_runningRegistrationJobIds.insert(jobId);
      m_registrationJobCancelFlags.emplace(jobId, std::move(cancelFlag));
      m_registrationJobIdsByTask.emplace(taskUid, jobId);
      m_registrationJobFutures.emplace(taskUid, std::move(future));
    }

    spdlog::info("Started registration job {}", jobId);
  }
}

void ImGuiWrapper::processRegistrationJobFutures()
{
  using namespace std::chrono_literals;

  std::vector<PendingRegistrationOutputLine> pendingOutputLines;
  {
    std::lock_guard<std::mutex> lock(m_registrationJobFuturesMutex);
    pendingOutputLines.swap(m_pendingRegistrationOutputLines);
  }
  for (auto& pendingLine : pendingOutputLines) {
    m_appData.registrationJobs().appendOutputLine(pendingLine.jobId, std::move(pendingLine.line));
  }

  std::vector<uuids::uuid> readyTasks;
  {
    std::lock_guard<std::mutex> lock(m_registrationJobFuturesMutex);
    for (auto& [taskUid, future] : m_registrationJobFutures) {
      if (future.valid() && std::future_status::ready == future.wait_for(0ms)) {
        readyTasks.push_back(taskUid);
      }
    }
  }

  for (const uuids::uuid& taskUid : readyTasks) {
    std::future<RegistrationJobTaskResult> future;
    std::string jobId;
    {
      std::lock_guard<std::mutex> lock(m_registrationJobFuturesMutex);
      auto futureIt = m_registrationJobFutures.find(taskUid);
      if (m_registrationJobFutures.end() == futureIt) {
        continue;
      }
      future = std::move(futureIt->second);
      m_registrationJobFutures.erase(futureIt);

      if (const auto jobIt = m_registrationJobIdsByTask.find(taskUid); m_registrationJobIdsByTask.end() != jobIt) {
        jobId = jobIt->second;
        m_registrationJobIdsByTask.erase(jobIt);
      }
    }

    RegistrationJobTaskResult result;
    try {
      result = future.get();
      if (jobId.empty()) {
        jobId = result.jobId;
      }
    }
    catch (const std::exception& e) {
      spdlog::error("Registration job task {} failed: {}", taskUid, e.what());
      if (!jobId.empty()) {
        registration::JobExecution failed;
        failed.status = registration::JobStatus::Failed;
        failed.errorMessage = e.what();
        m_appData.registrationJobs().applyExecution(jobId, failed);
      }
    }

    {
      std::lock_guard<std::mutex> lock(m_registrationJobFuturesMutex);
      if (!jobId.empty()) {
        m_runningRegistrationJobIds.erase(jobId);
        m_registrationJobCancelFlags.erase(jobId);
      }
    }

    if (!result.jobId.empty()) {
      if (const registration::JobRecord* job = m_appData.registrationJobs().find(result.jobId);
          job && job->status == registration::JobStatus::Cancelled)
      {
        spdlog::info("Registration job {} completed after cancellation; leaving job cancelled", result.jobId);
      }
      else {
        m_appData.registrationJobs().applyExecution(result.jobId, result.execution);
      }
      spdlog::info(
        "Registration job {} finished with status {}",
        result.jobId,
        registration::label(result.execution.status));
    }
  }

  if (!readyTasks.empty() && m_postEmptyGlfwEvent) {
    m_postEmptyGlfwEvent();
  }
}
