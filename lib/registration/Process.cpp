#include "registration/Process.h"

#include <array>
#include <cstdio>
#include <sstream>
#include <string>

#ifndef _WIN32
#include <sys/wait.h>
#endif

namespace registration
{
namespace
{

#ifdef _WIN32
std::string quoteForWindowsShell(const std::string& value)
{
  std::string quoted = "\"";
  for (const char ch : value) {
    if (ch == '"' || ch == '\\') {
      quoted.push_back('\\');
    }
    quoted.push_back(ch);
  }
  quoted.push_back('"');
  return quoted;
}
#else
std::string quoteForPosixShell(const std::string& value)
{
  if (value.empty()) {
    return "''";
  }

  std::string quoted = "'";
  for (const char ch : value) {
    if (ch == '\'') {
      quoted += "'\\''";
    }
    else {
      quoted.push_back(ch);
    }
  }
  quoted.push_back('\'');
  return quoted;
}
#endif

std::string shellQuote(const std::string& value)
{
#ifdef _WIN32
  return quoteForWindowsShell(value);
#else
  return quoteForPosixShell(value);
#endif
}

std::string commandLine(const CommandSpec& command)
{
  std::ostringstream stream;
  stream << shellQuote(command.executable);
  for (const std::string& arg : command.args) {
    stream << ' ' << shellQuote(arg);
  }
  return stream.str();
}

std::string shellCommand(const CommandSpec& command, const ProcessOptions& options)
{
  std::ostringstream stream;

  if (!options.workingDirectory.empty()) {
#ifdef _WIN32
    stream << "cd /d " << shellQuote(options.workingDirectory.string()) << " && ";
#else
    stream << "cd " << shellQuote(options.workingDirectory.string()) << " && ";
#endif
  }

#ifdef _WIN32
  for (const EnvironmentVariable& variable : options.environment) {
    stream << "set " << variable.name << "=" << variable.value << " && ";
  }
#else
  for (const EnvironmentVariable& variable : options.environment) {
    stream << variable.name << '=' << shellQuote(variable.value) << ' ';
  }
#endif

  stream << commandLine(command);
  if (options.mergeStdErrIntoStdOut) {
#ifdef _WIN32
    stream << " 2>&1";
#else
    stream << " 2>&1";
#endif
  }
  return stream.str();
}

int normalizedExitCode(int status)
{
#ifdef _WIN32
  return status;
#else
  if (WIFEXITED(status)) {
    return WEXITSTATUS(status);
  }
  if (WIFSIGNALED(status)) {
    return 128 + WTERMSIG(status);
  }
  return status;
#endif
}

} // namespace

ProcessResult
ShellProcessRunner::run(const CommandSpec& command, const ProcessOptions& options, const ProcessCallbacks& callbacks)
{
  ProcessResult result;
  ProcessOptions shellOptions = options;
  shellOptions.mergeStdErrIntoStdOut = true;

  const std::string commandText = shellCommand(command, shellOptions);

#ifdef _WIN32
  FILE* pipe = _popen(commandText.c_str(), "r");
#else
  FILE* pipe = popen(commandText.c_str(), "r");
#endif

  if (!pipe) {
    result.launchFailed = true;
    result.failureMessage = "Could not launch backend process.";
    return result;
  }

  std::array<char, 4096> buffer{};
  std::string partialLine;
  while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe)) {
    partialLine += buffer.data();
    std::size_t newlinePos = std::string::npos;
    while ((newlinePos = partialLine.find('\n')) != std::string::npos) {
      std::string line = partialLine.substr(0, newlinePos);
      if (!line.empty() && line.back() == '\r') {
        line.pop_back();
      }
      ProcessOutputLine outputLine{OutputStream::Stdout, std::move(line)};
      if (callbacks.onOutputLine) {
        callbacks.onOutputLine(outputLine);
      }
      else {
        result.outputLines.push_back(std::move(outputLine));
      }
      partialLine.erase(0, newlinePos + 1);
    }
  }

  if (!partialLine.empty()) {
    if (!partialLine.empty() && partialLine.back() == '\r') {
      partialLine.pop_back();
    }
    ProcessOutputLine outputLine{OutputStream::Stdout, std::move(partialLine)};
    if (callbacks.onOutputLine) {
      callbacks.onOutputLine(outputLine);
    }
    else {
      result.outputLines.push_back(std::move(outputLine));
    }
  }

#ifdef _WIN32
  const int status = _pclose(pipe);
#else
  const int status = pclose(pipe);
#endif
  result.exitCode = normalizedExitCode(status);
  return result;
}

} // namespace registration
