#include "registration/Process.h"

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <filesystem>
#include <string>
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

#ifdef _WIN32
TEST_CASE("shell process runner launches executables from paths with spaces", "[registration][process]")
{
  const auto uniqueSuffix = std::chrono::steady_clock::now().time_since_epoch().count();
  const std::filesystem::path testDirectory =
    std::filesystem::temp_directory_path() / ("entropy process runner test " + std::to_string(uniqueSuffix));
  const std::filesystem::path testExecutable = testDirectory / "cmake copy.exe";
  std::filesystem::create_directories(testDirectory);
  std::filesystem::copy_file(
    ENTROPY_TEST_CMAKE_COMMAND,
    testExecutable,
    std::filesystem::copy_options::overwrite_existing);

  registration::CommandSpec command;
  command.description = "test command";
  command.executable = testExecutable.string();
  command.args = {"-E", "echo", "path with spaces"};

  registration::ShellProcessRunner runner;
  const registration::ProcessResult result =
    runner.run(command, registration::ProcessOptions{}, registration::ProcessCallbacks{});

  CHECK_FALSE(result.launchFailed);
  CHECK(result.exitCode == 0);
  REQUIRE(result.outputLines.size() == 1);
  CHECK(result.outputLines.front().text == "path with spaces");
}
#endif

#ifndef _WIN32
TEST_CASE("shell process runner labels stdout and stderr separately", "[registration][process]")
{
  registration::ShellProcessRunner runner;
  std::vector<registration::ProcessOutputLine> streamedLines;

  registration::ProcessCallbacks callbacks;
  callbacks.onOutputLine = [&](const registration::ProcessOutputLine& line) {
    streamedLines.push_back(line);
  };

  registration::CommandSpec command;
  command.description = "stdout and stderr shell";
  command.executable = "/bin/sh";
  command.args = {"-c", "echo stdout-line; echo stderr-line >&2"};

  const registration::ProcessResult result = runner.run(command, registration::ProcessOptions{}, callbacks);

  CHECK_FALSE(result.launchFailed);
  CHECK(result.exitCode == 0);
  REQUIRE(streamedLines.size() == 2);
  CHECK(streamedLines[0].stream == registration::OutputStream::Stdout);
  CHECK(streamedLines[0].text == "stdout-line");
  CHECK(streamedLines[1].stream == registration::OutputStream::Stderr);
  CHECK(streamedLines[1].text == "stderr-line");
}

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
