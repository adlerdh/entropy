#include "registration/Process.h"

#include <algorithm>
#include <array>
#include <cerrno>
#include <cwchar>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#else
#include <csignal>
#include <fcntl.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace registration
{
namespace
{

#ifdef _WIN32
struct HandleCloser
{
  void operator()(HANDLE handle) const
  {
    if (handle && handle != INVALID_HANDLE_VALUE) {
      CloseHandle(handle);
    }
  }
};

using UniqueHandle = std::unique_ptr<std::remove_pointer_t<HANDLE>, HandleCloser>;

UniqueHandle makeHandle(HANDLE handle)
{
  return UniqueHandle{handle};
}

std::wstring widen(std::string_view value)
{
  if (value.empty()) {
    return {};
  }

  const int requiredSize =
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()), nullptr, 0);
  if (requiredSize <= 0) {
    return std::wstring{value.begin(), value.end()};
  }

  std::wstring result(static_cast<std::size_t>(requiredSize), L'\0');
  MultiByteToWideChar(
    CP_UTF8,
    MB_ERR_INVALID_CHARS,
    value.data(),
    static_cast<int>(value.size()),
    result.data(),
    requiredSize);
  return result;
}

std::string narrow(const std::wstring& value)
{
  if (value.empty()) {
    return {};
  }

  const int requiredSize =
    WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
  if (requiredSize <= 0) {
    return {};
  }

  std::string result(static_cast<std::size_t>(requiredSize), '\0');
  WideCharToMultiByte(
    CP_UTF8,
    0,
    value.data(),
    static_cast<int>(value.size()),
    result.data(),
    requiredSize,
    nullptr,
    nullptr);
  return result;
}

std::string windowsErrorMessage(const char* prefix, DWORD errorCode = GetLastError())
{
  LPWSTR messageBuffer = nullptr;
  const DWORD size = FormatMessageW(
    FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
    nullptr,
    errorCode,
    MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
    reinterpret_cast<LPWSTR>(&messageBuffer),
    0,
    nullptr);

  std::string message = prefix;
  if (size > 0 && messageBuffer) {
    std::wstring wideMessage{messageBuffer, size};
    while (!wideMessage.empty() &&
           (wideMessage.back() == L'\n' || wideMessage.back() == L'\r' || wideMessage.back() == L'.'))
    {
      wideMessage.pop_back();
    }
    message += ": ";
    message += narrow(wideMessage);
  }

  if (messageBuffer) {
    LocalFree(messageBuffer);
  }
  return message;
}

std::wstring quoteWindowsArgument(const std::wstring& value)
{
  if (value.empty()) {
    return L"\"\"";
  }

  bool needsQuotes = false;
  for (const wchar_t ch : value) {
    if (ch == L' ' || ch == L'\t' || ch == L'\n' || ch == L'\v' || ch == L'"') {
      needsQuotes = true;
      break;
    }
  }
  if (!needsQuotes) {
    return value;
  }

  std::wstring quoted = L"\"";
  std::size_t backslashes = 0;
  for (const wchar_t ch : value) {
    if (ch == L'\\') {
      ++backslashes;
      continue;
    }
    if (ch == L'"') {
      quoted.append(backslashes * 2 + 1, L'\\');
      quoted.push_back(ch);
      backslashes = 0;
      continue;
    }
    quoted.append(backslashes, L'\\');
    backslashes = 0;
    quoted.push_back(ch);
  }
  quoted.append(backslashes * 2, L'\\');
  quoted.push_back(L'"');
  return quoted;
}

std::wstring windowsCommandLine(const CommandSpec& command)
{
  std::wstring commandLine = quoteWindowsArgument(std::filesystem::path{command.executable}.wstring());
  for (const std::string& arg : command.args) {
    commandLine.push_back(L' ');
    commandLine += quoteWindowsArgument(widen(arg));
  }
  return commandLine;
}

struct CaseInsensitiveLess
{
  bool operator()(const std::wstring& lhs, const std::wstring& rhs) const
  {
    return _wcsicmp(lhs.c_str(), rhs.c_str()) < 0;
  }
};

std::vector<wchar_t> environmentBlock(const std::vector<EnvironmentVariable>& overrides)
{
  if (overrides.empty()) {
    return {};
  }

  std::map<std::wstring, std::wstring, CaseInsensitiveLess> environment;
  LPWCH strings = GetEnvironmentStringsW();
  if (strings) {
    for (LPCWCH current = strings; *current; current += std::wcslen(current) + 1) {
      const std::wstring entry{current};
      const std::size_t separator = entry.find(L'=');
      if (separator == std::wstring::npos || separator == 0) {
        continue;
      }
      environment[entry.substr(0, separator)] = entry.substr(separator + 1);
    }
    FreeEnvironmentStringsW(strings);
  }

  for (const EnvironmentVariable& variable : overrides) {
    environment[widen(variable.name)] = widen(variable.value);
  }

  std::vector<wchar_t> block;
  for (const auto& [name, value] : environment) {
    block.insert(block.end(), name.begin(), name.end());
    block.push_back(L'=');
    block.insert(block.end(), value.begin(), value.end());
    block.push_back(L'\0');
  }
  block.push_back(L'\0');
  return block;
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

std::string shellQuote(const std::string& value)
{
  return quoteForPosixShell(value);
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
    stream << "cd " << shellQuote(options.workingDirectory.string()) << " && ";
  }

  for (const EnvironmentVariable& variable : options.environment) {
    stream << variable.name << '=' << shellQuote(variable.value) << ' ';
  }

  stream << commandLine(command);
  if (options.mergeStdErrIntoStdOut) {
    stream << " 2>&1";
  }
  return stream.str();
}
#endif

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

#ifdef _WIN32
void consumeOutput(
  ProcessResult& result,
  const ProcessCallbacks& callbacks,
  std::string& partialLine,
  OutputStream stream,
  const char* data,
  std::size_t size)
{
  partialLine.append(data, size);
  std::size_t newlinePos = std::string::npos;
  while ((newlinePos = partialLine.find('\n')) != std::string::npos) {
    deliverOutputLine(result, callbacks, stream, partialLine.substr(0, newlinePos));
    partialLine.erase(0, newlinePos + 1);
  }
}
#else
void consumeOutput(
  ProcessResult& result,
  const ProcessCallbacks& callbacks,
  std::string& partialLine,
  OutputStream stream,
  const char* data,
  std::size_t size)
{
  partialLine.append(data, size);
  std::size_t newlinePos = std::string::npos;
  while ((newlinePos = partialLine.find('\n')) != std::string::npos) {
    deliverOutputLine(result, callbacks, stream, partialLine.substr(0, newlinePos));
    partialLine.erase(0, newlinePos + 1);
  }
}
#endif

bool cancellationRequested(const ProcessCallbacks& callbacks)
{
  return callbacks.shouldCancel && callbacks.shouldCancel();
}

#ifdef _WIN32
void readAvailablePipeOutput(
  HANDLE pipe,
  ProcessResult& result,
  const ProcessCallbacks& callbacks,
  std::string& partialLine,
  OutputStream stream)
{
  DWORD bytesAvailable = 0;
  while (PeekNamedPipe(pipe, nullptr, 0, nullptr, &bytesAvailable, nullptr) && bytesAvailable > 0) {
    std::array<char, 4096> buffer{};
    DWORD bytesRead = 0;
    if (
      !ReadFile(
        pipe,
        buffer.data(),
        static_cast<DWORD>(std::min<std::size_t>(buffer.size(), bytesAvailable)),
        &bytesRead,
        nullptr) ||
      bytesRead == 0)
    {
      break;
    }
    consumeOutput(result, callbacks, partialLine, stream, buffer.data(), bytesRead);
  }
}

ProcessResult
runWindowsProcess(const CommandSpec& command, const ProcessOptions& options, const ProcessCallbacks& callbacks)
{
  ProcessResult result;

  SECURITY_ATTRIBUTES securityAttributes{};
  securityAttributes.nLength = sizeof(securityAttributes);
  securityAttributes.bInheritHandle = TRUE;

  HANDLE stdoutReadRaw = nullptr;
  HANDLE stdoutWriteRaw = nullptr;
  if (!CreatePipe(&stdoutReadRaw, &stdoutWriteRaw, &securityAttributes, 0)) {
    result.launchFailed = true;
    result.failureMessage = windowsErrorMessage("Could not create backend stdout pipe");
    return result;
  }
  UniqueHandle stdoutRead = makeHandle(stdoutReadRaw);
  UniqueHandle stdoutWrite = makeHandle(stdoutWriteRaw);
  SetHandleInformation(stdoutRead.get(), HANDLE_FLAG_INHERIT, 0);

  UniqueHandle stderrRead;
  UniqueHandle stderrWrite;
  if (!options.mergeStdErrIntoStdOut) {
    HANDLE stderrReadRaw = nullptr;
    HANDLE stderrWriteRaw = nullptr;
    if (!CreatePipe(&stderrReadRaw, &stderrWriteRaw, &securityAttributes, 0)) {
      result.launchFailed = true;
      result.failureMessage = windowsErrorMessage("Could not create backend stderr pipe");
      return result;
    }
    stderrRead = makeHandle(stderrReadRaw);
    stderrWrite = makeHandle(stderrWriteRaw);
    SetHandleInformation(stderrRead.get(), HANDLE_FLAG_INHERIT, 0);
  }

  STARTUPINFOW startupInfo{};
  startupInfo.cb = sizeof(startupInfo);
  startupInfo.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
  startupInfo.wShowWindow = SW_HIDE;
  startupInfo.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
  startupInfo.hStdOutput = stdoutWrite.get();
  startupInfo.hStdError = options.mergeStdErrIntoStdOut ? stdoutWrite.get() : stderrWrite.get();

  PROCESS_INFORMATION processInformation{};
  std::wstring commandLineText = windowsCommandLine(command);
  std::vector<wchar_t> mutableCommandLine(commandLineText.begin(), commandLineText.end());
  mutableCommandLine.push_back(L'\0');
  std::vector<wchar_t> environment = environmentBlock(options.environment);
  std::wstring workingDirectory =
    options.workingDirectory.empty() ? std::wstring{} : options.workingDirectory.wstring();

  const BOOL created = CreateProcessW(
    nullptr,
    mutableCommandLine.data(),
    nullptr,
    nullptr,
    TRUE,
    CREATE_NO_WINDOW | (environment.empty() ? 0 : CREATE_UNICODE_ENVIRONMENT),
    environment.empty() ? nullptr : environment.data(),
    workingDirectory.empty() ? nullptr : workingDirectory.c_str(),
    &startupInfo,
    &processInformation);

  if (!created) {
    result.launchFailed = true;
    result.failureMessage = windowsErrorMessage("Could not launch backend process");
    return result;
  }

  UniqueHandle process = makeHandle(processInformation.hProcess);
  UniqueHandle thread = makeHandle(processInformation.hThread);
  stdoutWrite.reset();
  stderrWrite.reset();

  std::string stdoutPartialLine;
  std::string stderrPartialLine;
  bool processExited = false;
  while (!processExited) {
    if (cancellationRequested(callbacks)) {
      result.cancelled = true;
      TerminateProcess(process.get(), 1);
    }

    readAvailablePipeOutput(stdoutRead.get(), result, callbacks, stdoutPartialLine, OutputStream::Stdout);
    if (stderrRead) {
      readAvailablePipeOutput(stderrRead.get(), result, callbacks, stderrPartialLine, OutputStream::Stderr);
    }

    const DWORD waitResult = WaitForSingleObject(process.get(), 100);
    processExited = waitResult == WAIT_OBJECT_0 || waitResult == WAIT_FAILED;
  }

  readAvailablePipeOutput(stdoutRead.get(), result, callbacks, stdoutPartialLine, OutputStream::Stdout);
  if (stderrRead) {
    readAvailablePipeOutput(stderrRead.get(), result, callbacks, stderrPartialLine, OutputStream::Stderr);
  }

  if (!stdoutPartialLine.empty()) {
    deliverOutputLine(result, callbacks, OutputStream::Stdout, std::move(stdoutPartialLine));
  }
  if (!stderrPartialLine.empty()) {
    deliverOutputLine(result, callbacks, OutputStream::Stderr, std::move(stderrPartialLine));
  }

  DWORD exitCode = 1;
  if (GetExitCodeProcess(process.get(), &exitCode)) {
    result.exitCode = static_cast<int>(exitCode);
  }
  else {
    result.exitCode = 1;
    result.failureMessage = windowsErrorMessage("Could not read backend process exit code");
  }

  return result;
}
#else
bool setNonBlocking(int fd)
{
  const int flags = fcntl(fd, F_GETFL, 0);
  return flags >= 0 && fcntl(fd, F_SETFL, flags | O_NONBLOCK) == 0;
}

void closeIfOpen(int& fd)
{
  if (fd >= 0) {
    close(fd);
    fd = -1;
  }
}

void readAvailableFdOutput(
  int fd,
  ProcessResult& result,
  const ProcessCallbacks& callbacks,
  std::string& partialLine,
  OutputStream stream)
{
  std::array<char, 4096> buffer{};
  while (true) {
    const ssize_t bytesRead = read(fd, buffer.data(), buffer.size());
    if (bytesRead > 0) {
      consumeOutput(result, callbacks, partialLine, stream, buffer.data(), static_cast<std::size_t>(bytesRead));
      continue;
    }
    break;
  }
}

ProcessResult
runPosixShellCommand(const CommandSpec& command, const ProcessOptions& options, const ProcessCallbacks& callbacks)
{
  ProcessResult result;
  const std::string commandText = shellCommand(command, options);

  int stdoutPipe[2] = {-1, -1};
  int stderrPipe[2] = {-1, -1};

  auto failWithPipeError = [&](const char* message) {
    closeIfOpen(stdoutPipe[0]);
    closeIfOpen(stdoutPipe[1]);
    closeIfOpen(stderrPipe[0]);
    closeIfOpen(stderrPipe[1]);
    result.launchFailed = true;
    result.failureMessage = message;
  };

  if (pipe(stdoutPipe) != 0) {
    failWithPipeError("Could not create backend stdout pipe.");
    return result;
  }
  if (!options.mergeStdErrIntoStdOut && pipe(stderrPipe) != 0) {
    failWithPipeError("Could not create backend stderr pipe.");
    return result;
  }

  const pid_t pid = fork();
  if (pid < 0) {
    failWithPipeError("Could not fork backend process.");
    return result;
  }

  if (pid == 0) {
    setpgid(0, 0);
    close(stdoutPipe[0]);
    dup2(stdoutPipe[1], STDOUT_FILENO);
    if (options.mergeStdErrIntoStdOut) {
      dup2(stdoutPipe[1], STDERR_FILENO);
    }
    else {
      close(stderrPipe[0]);
      dup2(stderrPipe[1], STDERR_FILENO);
      close(stderrPipe[1]);
    }
    close(stdoutPipe[1]);
    execl("/bin/sh", "sh", "-c", commandText.c_str(), static_cast<char*>(nullptr));
    _exit(127);
  }

  closeIfOpen(stdoutPipe[1]);
  closeIfOpen(stderrPipe[1]);
  setpgid(pid, pid);

  setNonBlocking(stdoutPipe[0]);
  if (!options.mergeStdErrIntoStdOut) {
    setNonBlocking(stderrPipe[0]);
  }

  std::string stdoutPartialLine;
  std::string stderrPartialLine;
  bool processExited = false;
  int status = 0;
  while (!processExited) {
    if (cancellationRequested(callbacks)) {
      result.cancelled = true;
      kill(-pid, SIGTERM);
    }

    fd_set readSet;
    FD_ZERO(&readSet);
    FD_SET(stdoutPipe[0], &readSet);
    int maxFd = stdoutPipe[0];
    if (stderrPipe[0] >= 0) {
      FD_SET(stderrPipe[0], &readSet);
      maxFd = std::max(maxFd, stderrPipe[0]);
    }
    timeval timeout{0, 100000};
    const int selectResult = select(maxFd + 1, &readSet, nullptr, nullptr, &timeout);
    if (selectResult > 0) {
      if (FD_ISSET(stdoutPipe[0], &readSet)) {
        readAvailableFdOutput(stdoutPipe[0], result, callbacks, stdoutPartialLine, OutputStream::Stdout);
      }
      if (stderrPipe[0] >= 0 && FD_ISSET(stderrPipe[0], &readSet)) {
        readAvailableFdOutput(stderrPipe[0], result, callbacks, stderrPartialLine, OutputStream::Stderr);
      }
    }

    const pid_t waitResult = waitpid(pid, &status, WNOHANG);
    if (waitResult == pid || (waitResult < 0 && errno == ECHILD)) {
      processExited = true;
    }
  }

  readAvailableFdOutput(stdoutPipe[0], result, callbacks, stdoutPartialLine, OutputStream::Stdout);
  if (stderrPipe[0] >= 0) {
    readAvailableFdOutput(stderrPipe[0], result, callbacks, stderrPartialLine, OutputStream::Stderr);
  }
  closeIfOpen(stdoutPipe[0]);
  closeIfOpen(stderrPipe[0]);

  if (result.cancelled) {
    kill(-pid, SIGKILL);
    waitpid(pid, &status, 0);
  }

  if (!stdoutPartialLine.empty()) {
    deliverOutputLine(result, callbacks, OutputStream::Stdout, std::move(stdoutPartialLine));
  }
  if (!stderrPartialLine.empty()) {
    deliverOutputLine(result, callbacks, OutputStream::Stderr, std::move(stderrPartialLine));
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
  return runWindowsProcess(command, options, callbacks);
#endif
}

} // namespace registration
