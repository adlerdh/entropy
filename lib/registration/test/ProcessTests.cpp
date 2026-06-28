#include "registration/Process.h"

#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <vector>

namespace
{

registration::CommandSpec cmakeCommand(std::vector<std::string> args)
{
  registration::CommandSpec command;
  command.description = "test command";
  command.executable = ENTROPY_TEST_CMAKE_COMMAND;
  command.args = std::move(args);
  return command;
}

} // namespace

TEST_CASE("shell process runner captures command output", "[registration][process]")
{
  registration::ShellProcessRunner runner;
  registration::ProcessOptions options;
  options.workingDirectory = std::filesystem::temp_directory_path();

  std::vector<registration::ProcessOutputLine> streamedLines;
  registration::ProcessCallbacks callbacks;
  callbacks.onOutputLine = [&](const registration::ProcessOutputLine& line) {
    streamedLines.push_back(line);
  };

  const registration::ProcessResult result =
    runner.run(cmakeCommand({"-E", "echo", "registration process test"}), options, callbacks);

  CHECK_FALSE(result.launchFailed);
  CHECK(result.exitCode == 0);
  REQUIRE(streamedLines.size() == 1);
  CHECK(streamedLines.front().stream == registration::OutputStream::Stdout);
  CHECK(streamedLines.front().text == "registration process test");
  CHECK(result.outputLines.empty());
}

TEST_CASE("shell process runner reports non-zero exit status", "[registration][process]")
{
  registration::ShellProcessRunner runner;
  const registration::ProcessResult result =
    runner.run(cmakeCommand({"-E", "false"}), registration::ProcessOptions{}, registration::ProcessCallbacks{});

  CHECK_FALSE(result.launchFailed);
  CHECK(result.exitCode != 0);
}

#ifndef _WIN32
TEST_CASE("shell process runner cancels an already running process", "[registration][process]")
{
  registration::ShellProcessRunner runner;
  bool processStarted = false;
  registration::ProcessCallbacks callbacks;
  callbacks.onOutputLine = [&](const registration::ProcessOutputLine& line) {
    if (line.text == "ready") {
      processStarted = true;
    }
  };
  callbacks.shouldCancel = [&]() {
    return processStarted;
  };

  registration::CommandSpec command;
  command.description = "cancellable shell";
  command.executable = "/bin/sh";
  command.args = {"-c", "echo ready; sleep 5"};

  const registration::ProcessResult result = runner.run(command, registration::ProcessOptions{}, callbacks);

  CHECK(result.cancelled);
  CHECK_FALSE(result.launchFailed);
  CHECK(result.exitCode != 0);
}
#endif
