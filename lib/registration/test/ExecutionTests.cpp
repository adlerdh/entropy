#include "registration/Execution.h"
#include "registration/Artifacts.h"
#include "registration/Json.h"
#include "registration/Progress.h"

#include <catch2/catch_test_macros.hpp>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <utility>

namespace
{

class ScriptedRunner final : public registration::IProcessRunner
{
public:
  std::vector<registration::ProcessResult> results;
  std::vector<registration::CommandSpec> commands;
  std::vector<registration::ProcessOptions> options;

  registration::ProcessResult run(
    const registration::CommandSpec& command,
    const registration::ProcessOptions& processOptions,
    const registration::ProcessCallbacks& callbacks) override
  {
    commands.push_back(command);
    options.push_back(processOptions);
    REQUIRE_FALSE(results.empty());
    registration::ProcessResult result = results.front();
    results.erase(results.begin());
    for (const registration::ProcessOutputLine& line : result.outputLines) {
      if (callbacks.onOutputLine) {
        callbacks.onOutputLine(line);
      }
    }
    result.outputLines.clear();
    return result;
  }
};

registration::DataRef imageRef(std::string uid, std::string fileName)
{
  registration::DataRef ref;
  ref.uid = std::move(uid);
  ref.displayName = ref.uid;
  ref.fileName = std::move(fileName);
  ref.source = registration::DataSource::LoadedImage;
  return ref;
}

registration::JobSpec jobForOneCommand(const std::string& suffix = "case")
{
  registration::JobSpec job;
  job.backend = registration::Backend::FireANTs;
  job.fixedImage = imageRef("fixed", "fixed.nii.gz");
  job.movingImage = imageRef("moving", "moving.nii.gz");
  job.outputDirectory = std::filesystem::temp_directory_path() / ("entropy-registration-execution-tests-" + suffix);
  job.outputPrefix = "case";
  std::filesystem::remove_all(job.outputDirectory);
  return job;
}

registration::ProcessOutputLine stdoutLine(const std::string& text)
{
  return registration::ProcessOutputLine{registration::OutputStream::Stdout, text};
}

registration::ProcessOutputLine stderrLine(const std::string& text)
{
  return registration::ProcessOutputLine{registration::OutputStream::Stderr, text};
}

} // namespace

TEST_CASE("registration job execution reports progress and completion", "[registration][execution]")
{
  registration::ProgressEvent event;
  event.kind = registration::ProgressEventKind::Progress;
  event.stageName = "deformable";
  event.iteration = 2;
  event.iterations = 10;
  event.progress = 0.2;

  ScriptedRunner runner;
  registration::ProcessResult success;
  success.exitCode = 0;
  success.outputLines = {stdoutLine(registration::progressEventLine(event))};
  runner.results.push_back(success);

  std::vector<registration::JobStatus> statuses;
  registration::JobExecutionCallbacks callbacks;
  callbacks.onStatusChanged = [&](registration::JobStatus status) {
    statuses.push_back(status);
  };

  const registration::JobSpec job = jobForOneCommand("progress");
  const registration::JobExecution execution = registration::executeJob(job, runner, callbacks);

  CHECK(execution.status == registration::JobStatus::Completed);
  REQUIRE(execution.manifest);
  CHECK(execution.manifest->success);
  CHECK(execution.manifest->warpedImage == registration::artifactPath(job, registration::ArtifactRole::WarpedImage));
  REQUIRE(execution.progressEvents.size() == 1);
  REQUIRE(execution.outputLines.size() == 1);
  CHECK(execution.outputLines.front().text == registration::progressEventLine(event));
  CHECK(execution.progressEvents.front().stageName == "deformable");
  REQUIRE_FALSE(statuses.empty());
  CHECK(statuses.back() == registration::JobStatus::Completed);
}

TEST_CASE("registration job execution writes FireANTs job spec before launch", "[registration][execution]")
{
  ScriptedRunner runner;
  registration::ProcessResult success;
  success.exitCode = 0;
  runner.results.push_back(success);

  const registration::JobSpec job = jobForOneCommand("writes-spec");
  const registration::JobExecution execution = registration::executeJob(job, runner);

  CHECK(execution.status == registration::JobStatus::Completed);

  std::ifstream stream(registration::artifactPath(job, registration::ArtifactRole::JobSpec));
  REQUIRE(stream);

  nlohmann::json json;
  stream >> json;
  const registration::JobSpec restored = json.get<registration::JobSpec>();
  CHECK(restored.fixedImage.uid == "fixed");
  CHECK(restored.movingImage.uid == "moving");
}

TEST_CASE("registration job execution exposes the FireANTs bridge on PYTHONPATH", "[registration][execution]")
{
  ScriptedRunner runner;
  registration::ProcessResult success;
  success.exitCode = 0;
  runner.results.push_back(success);

  const registration::JobSpec job = jobForOneCommand("fireants-pythonpath");
  const registration::JobExecution execution = registration::executeJob(job, runner);

  CHECK(execution.status == registration::JobStatus::Completed);
  REQUIRE(runner.options.size() == 1);
  const auto& environment = runner.options.front().environment;
  const auto it =
    std::find_if(environment.begin(), environment.end(), [](const registration::EnvironmentVariable& variable) {
      return variable.name == "PYTHONPATH";
    });
  REQUIRE(it != environment.end());
  CHECK(it->value.find("registration") != std::string::npos);
}

TEST_CASE("registration job execution reads backend result manifest when present", "[registration][execution]")
{
  registration::JobSpec job = jobForOneCommand("reads-manifest");
  std::filesystem::create_directories(job.outputDirectory);

  registration::ResultManifest backendManifest;
  backendManifest.success = true;
  backendManifest.backend = registration::Backend::FireANTs;
  backendManifest.fixedImageUid = "fixed";
  backendManifest.movingImageUid = "moving";
  backendManifest.warpedImage = "backend-warped.nii.gz";

  {
    std::ofstream stream(registration::artifactPath(job, registration::ArtifactRole::ResultManifest));
    const nlohmann::json json = backendManifest;
    stream << json.dump(2) << '\n';
  }

  ScriptedRunner runner;
  registration::ProcessResult success;
  success.exitCode = 0;
  runner.results.push_back(success);

  const registration::JobExecution execution = registration::executeJob(job, runner);

  REQUIRE(execution.manifest);
  CHECK(execution.manifest->success);
  CHECK(execution.manifest->warpedImage == "backend-warped.nii.gz");
}

TEST_CASE("registration job execution stops after a failed command", "[registration][execution]")
{
  ScriptedRunner runner;
  registration::ProcessResult failed;
  failed.exitCode = 2;
  failed.outputLines = {stderrLine("metric diverged")};
  runner.results.push_back(failed);

  const registration::JobExecution execution = registration::executeJob(jobForOneCommand(), runner);

  CHECK(execution.status == registration::JobStatus::Failed);
  CHECK_FALSE(execution.errorMessage.empty());
  REQUIRE(execution.commands.size() == 1);
  CHECK(execution.commands.front().result.exitCode == 2);
  REQUIRE_FALSE(execution.warnings.empty());
  CHECK(execution.warnings.front() == "metric diverged");
  REQUIRE(execution.outputLines.size() == 1);
  CHECK(execution.outputLines.front().text == "metric diverged");
}

TEST_CASE("registration job execution reports cancelled process", "[registration][execution]")
{
  ScriptedRunner runner;
  registration::ProcessResult cancelled;
  cancelled.cancelled = true;
  runner.results.push_back(cancelled);

  const registration::JobExecution execution = registration::executeJob(jobForOneCommand(), runner);

  CHECK(execution.status == registration::JobStatus::Cancelled);
  CHECK(execution.errorMessage == "Registration was cancelled.");
}

TEST_CASE("registration job execution honors cancellation before launching a command", "[registration][execution]")
{
  ScriptedRunner runner;
  registration::ProcessResult success;
  success.exitCode = 0;
  runner.results.push_back(success);

  registration::JobExecutionCallbacks callbacks;
  callbacks.shouldCancel = [] {
    return true;
  };

  const registration::JobExecution execution =
    registration::executeJob(jobForOneCommand("cancel-before-launch"), runner, callbacks);

  CHECK(execution.status == registration::JobStatus::Cancelled);
  CHECK(runner.commands.empty());
  CHECK(execution.errorMessage == "Registration was cancelled.");
}

TEST_CASE("registration job execution uses configured command options", "[registration][execution]")
{
  ScriptedRunner runner;
  registration::ProcessResult success;
  success.exitCode = 0;
  runner.results.push_back(success);

  registration::CommandGenerationOptions options;
  options.fireAntsPythonExecutable = "/venv/bin/python";
  options.fireAntsBridgeModule = "custom_bridge";

  const registration::JobExecution execution = registration::executeJob(jobForOneCommand(), options, runner);

  CHECK(execution.status == registration::JobStatus::Completed);
  REQUIRE(runner.commands.size() == 1);
  CHECK(runner.commands.front().executable == "/venv/bin/python");
  REQUIRE(runner.commands.front().args.size() > 1);
  CHECK(runner.commands.front().args.at(1) == "custom_bridge");
}
