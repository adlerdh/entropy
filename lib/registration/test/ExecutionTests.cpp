#include "registration/Execution.h"
#include "registration/Progress.h"

#include <catch2/catch_test_macros.hpp>

#include <utility>

namespace
{

class ScriptedRunner final : public registration::IProcessRunner
{
public:
  std::vector<registration::ProcessResult> results;
  std::vector<registration::CommandSpec> commands;

  registration::ProcessResult run(
    const registration::CommandSpec& command,
    const registration::ProcessOptions&,
    const registration::ProcessCallbacks& callbacks) override
  {
    commands.push_back(command);
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

registration::JobSpec jobForOneCommand()
{
  registration::JobSpec job;
  job.backend = registration::Backend::FireANTs;
  job.fixedImage = imageRef("fixed", "fixed.nii.gz");
  job.movingImage = imageRef("moving", "moving.nii.gz");
  job.outputDirectory = "/tmp/reg";
  job.outputPrefix = "case";
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

  const registration::JobExecution execution = registration::executeJob(jobForOneCommand(), runner, callbacks);

  CHECK(execution.status == registration::JobStatus::Completed);
  REQUIRE(execution.progressEvents.size() == 1);
  CHECK(execution.progressEvents.front().stageName == "deformable");
  REQUIRE_FALSE(statuses.empty());
  CHECK(statuses.back() == registration::JobStatus::Completed);
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
