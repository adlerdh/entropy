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
  Command,
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
 * @brief One output-log line emitted by Entropy or a backend process.
 */
struct ProcessOutputLine
{
  OutputStream stream = OutputStream::Stdout; //!< Source stream or Entropy command marker.
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
  std::function<bool()> shouldCancel;                         //!< True when the process should be cancelled.
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

/**
 * @brief Process runner that launches backend commands through the platform process API.
 *
 * This runner is
 * intentionally isolated behind IProcessRunner. On Windows it uses CreateProcessW with redirected
 * pipes and no
 * visible console window; on POSIX it uses the platform shell with redirected output.
 */
class ShellProcessRunner final : public IProcessRunner
{
public:
  /**
   * @brief Run one process command and capture stdout/stderr output.
   * @param command Command to execute.
   * @param options Launch options.
   * @param callbacks Streaming callbacks.
   * @return Completed process result.
   */
  ProcessResult run(const CommandSpec& command, const ProcessOptions& options, const ProcessCallbacks& callbacks)
    override;
};

} // namespace registration
