#pragma once

#include "registration/Commands.h"

#include <filesystem>
#include <functional>
#include <string>
#include <vector>

namespace registration
{

/**
 * @brief Stream that produced a process output line.
 */
enum class OutputStream
{
  Stdout,
  Stderr
};

/**
 * @brief Environment variable override for a backend process.
 */
struct EnvironmentVariable
{
  std::string name;  //!< Variable name.
  std::string value; //!< Variable value.
};

/**
 * @brief Options used when launching one backend process.
 */
struct ProcessOptions
{
  std::filesystem::path workingDirectory;       //!< Process working directory.
  std::vector<EnvironmentVariable> environment; //!< Environment overrides.
  bool mergeStdErrIntoStdOut = false;           //!< True to treat stderr lines as stdout lines.
};

/**
 * @brief One line emitted by a backend process.
 */
struct ProcessOutputLine
{
  OutputStream stream = OutputStream::Stdout; //!< Source stream.
  std::string text;                           //!< Line text without trailing newline.
};

/**
 * @brief Completed backend process result.
 */
struct ProcessResult
{
  int exitCode = 0;           //!< Native process exit code.
  bool cancelled = false;     //!< True when Entropy cancelled the process.
  bool launchFailed = false;  //!< True when the process could not be started.
  std::string failureMessage; //!< Launch/runtime failure explanation.
  /**
   * @brief Captured stdout/stderr lines in observed order.
   *
   * If the runner streams output through ProcessCallbacks while the process is running, this should contain only
   * lines that were not already delivered through those callbacks.
   */
  std::vector<ProcessOutputLine> outputLines;
};

/**
 * @brief Callbacks invoked while one backend process runs.
 */
struct ProcessCallbacks
{
  std::function<void(const ProcessOutputLine&)> onOutputLine; //!< Called for every output line.
};

/**
 * @brief Abstract process runner used by registration job orchestration.
 */
class IProcessRunner
{
public:
  virtual ~IProcessRunner() = default;

  /**
   * @brief Run one process command.
   * @param command Command to execute.
   * @param options Launch options.
   * @param callbacks Streaming callbacks.
   * @return Completed process result.
   */
  virtual ProcessResult
  run(const CommandSpec& command, const ProcessOptions& options, const ProcessCallbacks& callbacks) = 0;
};

} // namespace registration
