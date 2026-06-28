#include "registration/Process.h"

#include <array>
#include <cerrno>
#include <cstdio>
#include <sstream>
#include <string>

#ifndef _WIN32
#include <csignal>
#include <fcntl.h>
#include <sys/select.h>
#include <sys/wait.h>
#include <unistd.h>
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

#ifndef _WIN32
void deliverOutputLine(ProcessResult& result, const ProcessCallbacks& callbacks, OutputStream stream, std::string line)
{
  if (!line.empty() && line.back() == '\r') {
    line.pop_back();
  }

  ProcessOutputLine outputLine{stream, std::move(line)};
  if (callbacks.onOutputLine) {
    callbacks.onOutputLine(outputLine);
  }
  else {
    result.outputLines.push_back(std::move(outputLine));
  }
}

void consumeOutput(
  ProcessResult& result,
  const ProcessCallbacks& callbacks,
  std::string& partialLine,
  const char* data,
  std::size_t size)
{
  partialLine.append(data, size);
  std::size_t newlinePos = std::string::npos;
  while ((newlinePos = partialLine.find('\n')) != std::string::npos) {
    deliverOutputLine(result, callbacks, OutputStream::Stdout, partialLine.substr(0, newlinePos));
    partialLine.erase(0, newlinePos + 1);
  }
}

bool cancellationRequested(const ProcessCallbacks& callbacks)
{
  return callbacks.shouldCancel && callbacks.shouldCancel();
}

ProcessResult
runPosixShellCommand(const CommandSpec& command, const ProcessOptions& options, const ProcessCallbacks& callbacks)
{
  ProcessResult result;
  ProcessOptions shellOptions = options;
  shellOptions.mergeStdErrIntoStdOut = true;
  const std::string commandText = shellCommand(command, shellOptions);

  int pipeFds[2] = {-1, -1};
  if (pipe(pipeFds) != 0) {
    result.launchFailed = true;
    result.failureMessage = "Could not create backend process pipe.";
    return result;
  }

  const pid_t pid = fork();
  if (pid < 0) {
    close(pipeFds[0]);
    close(pipeFds[1]);
    result.launchFailed = true;
    result.failureMessage = "Could not fork backend process.";
    return result;
  }

  if (pid == 0) {
    setpgid(0, 0);
    close(pipeFds[0]);
    dup2(pipeFds[1], STDOUT_FILENO);
    dup2(pipeFds[1], STDERR_FILENO);
    close(pipeFds[1]);
    execl("/bin/sh", "sh", "-c", commandText.c_str(), static_cast<char*>(nullptr));
    _exit(127);
  }

  close(pipeFds[1]);
  setpgid(pid, pid);

  int flags = fcntl(pipeFds[0], F_GETFL, 0);
  if (flags >= 0) {
    fcntl(pipeFds[0], F_SETFL, flags | O_NONBLOCK);
  }

  std::string partialLine;
  bool processExited = false;
  int status = 0;
  while (!processExited) {
    if (cancellationRequested(callbacks)) {
      result.cancelled = true;
      kill(-pid, SIGTERM);
    }

    fd_set readSet;
    FD_ZERO(&readSet);
    FD_SET(pipeFds[0], &readSet);
    timeval timeout{0, 100000};
    const int selectResult = select(pipeFds[0] + 1, &readSet, nullptr, nullptr, &timeout);
    if (selectResult > 0 && FD_ISSET(pipeFds[0], &readSet)) {
      std::array<char, 4096> buffer{};
      while (true) {
        const ssize_t bytesRead = read(pipeFds[0], buffer.data(), buffer.size());
        if (bytesRead > 0) {
          consumeOutput(result, callbacks, partialLine, buffer.data(), static_cast<std::size_t>(bytesRead));
          continue;
        }
        if (bytesRead == 0 || (bytesRead < 0 && errno != EAGAIN && errno != EWOULDBLOCK)) {
          break;
        }
        break;
      }
    }

    const pid_t waitResult = waitpid(pid, &status, WNOHANG);
    if (waitResult == pid) {
      processExited = true;
    }
    else if (waitResult < 0 && errno == ECHILD) {
      processExited = true;
    }
  }

  std::array<char, 4096> buffer{};
  while (true) {
    const ssize_t bytesRead = read(pipeFds[0], buffer.data(), buffer.size());
    if (bytesRead > 0) {
      consumeOutput(result, callbacks, partialLine, buffer.data(), static_cast<std::size_t>(bytesRead));
      continue;
    }
    break;
  }
  close(pipeFds[0]);

  if (result.cancelled) {
    kill(-pid, SIGKILL);
    waitpid(pid, &status, 0);
  }

  if (!partialLine.empty()) {
    deliverOutputLine(result, callbacks, OutputStream::Stdout, std::move(partialLine));
  }

  result.exitCode = normalizedExitCode(status);
  return result;
}
#endif

} // namespace

ProcessResult
ShellProcessRunner::run(const CommandSpec& command, const ProcessOptions& options, const ProcessCallbacks& callbacks)
{
#ifndef _WIN32
  return runPosixShellCommand(command, options, callbacks);
#else
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
#endif
}

} // namespace registration
