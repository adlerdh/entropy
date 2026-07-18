#include "logic/app/StackTrace.h"

#include <algorithm>
#include <cstdlib>
#include <exception>
#include <iterator>
#include <sstream>

#if defined(_WIN32)
#define NOMINMAX
#include <windows.h>
#else
#include <cerrno>
#include <csignal>
#include <cstring>
#include <execinfo.h>
#include <unistd.h>
#endif

namespace
{
#if !defined(_WIN32)
void writeToStderr(const char* text, std::size_t size)
{
  while (size > 0) {
    const ssize_t written = ::write(STDERR_FILENO, text, size);
    if (written > 0) {
      text += written;
      size -= static_cast<std::size_t>(written);
      continue;
    }
    if (written < 0 && errno == EINTR) {
      continue;
    }
    break;
  }
}
#endif

[[noreturn]] void terminateWithTrace()
{
  std::ostringstream message;
  message << "Unhandled exception or terminate() called\n";
  if (const std::exception_ptr exception = std::current_exception()) {
    try {
      std::rethrow_exception(exception);
    }
    catch (const std::exception& e) {
      message << "Exception: " << e.what() << '\n';
    }
    catch (...) {
      message << "Exception: unknown\n";
    }
  }

  message << stack_trace::current(1);
  const std::string text = message.str();

#if defined(_WIN32)
  DWORD written = 0;
  WriteFile(GetStdHandle(STD_ERROR_HANDLE), text.data(), static_cast<DWORD>(text.size()), &written, nullptr);
  TerminateProcess(GetCurrentProcess(), EXIT_FAILURE);
#else
  writeToStderr(text.data(), text.size());
  std::_Exit(EXIT_FAILURE);
#endif
}

#if defined(_WIN32)
LONG WINAPI unhandledExceptionFilter(EXCEPTION_POINTERS*)
{
  const std::string text = "Fatal structured exception\n" + stack_trace::current(1);
  DWORD written = 0;
  WriteFile(GetStdHandle(STD_ERROR_HANDLE), text.data(), static_cast<DWORD>(text.size()), &written, nullptr);
  return EXCEPTION_EXECUTE_HANDLER;
}
#else
const char* signalName(int signal)
{
  switch (signal) {
    case SIGABRT:
      return "SIGABRT";
    case SIGBUS:
      return "SIGBUS";
    case SIGFPE:
      return "SIGFPE";
    case SIGILL:
      return "SIGILL";
    case SIGSEGV:
      return "SIGSEGV";
    default:
      return "signal";
  }
}

void writeLiteral(const char* text)
{
  writeToStderr(text, std::strlen(text));
}

void crashSignalHandler(int signal)
{
  writeLiteral("\nFatal ");
  writeLiteral(signalName(signal));
  writeLiteral("\n");

  void* frames[64];
  const int frameCount = ::backtrace(frames, static_cast<int>(std::size(frames)));
  ::backtrace_symbols_fd(frames, frameCount, STDERR_FILENO);

  std::signal(signal, SIG_DFL);
  std::raise(signal);
}
#endif
} // namespace

namespace stack_trace
{
std::string current(unsigned int skipFrames)
{
#if defined(_WIN32)
  void* frames[64];
  const USHORT frameCount =
    CaptureStackBackTrace(static_cast<DWORD>(skipFrames + 1), static_cast<DWORD>(std::size(frames)), frames, nullptr);

  std::ostringstream out;
  out << "Module base: " << GetModuleHandleW(nullptr) << '\n';
  out << "Stack trace:\n";
  for (USHORT i = 0; i < frameCount; ++i) {
    out << "  [" << i << "] " << frames[i] << '\n';
  }
  return out.str();
#else
  void* frames[64];
  const int frameCount = ::backtrace(frames, static_cast<int>(std::size(frames)));
  char** symbols = ::backtrace_symbols(frames, frameCount);

  if (!symbols) {
    return "Stack trace unavailable\n";
  }

  std::ostringstream out;
  out << "Stack trace:\n";
  const int firstFrame = std::min<int>(frameCount, static_cast<int>(skipFrames + 1));
  for (int i = firstFrame; i < frameCount; ++i) {
    out << "  [" << (i - firstFrame) << "] " << symbols[i] << '\n';
  }

  std::free(static_cast<void*>(symbols));
  return out.str();
#endif
}

void installCrashHandlers()
{
  std::set_terminate(terminateWithTrace);

#if defined(_WIN32)
  SetUnhandledExceptionFilter(unhandledExceptionFilter);
#else
  // Preload the platform backtrace machinery before a crash path. The signal handler still only
  // provides best-effort diagnostics, but this avoids common first-use loader work in the handler.
  (void)current();

  std::signal(SIGABRT, crashSignalHandler);
  std::signal(SIGBUS, crashSignalHandler);
  std::signal(SIGFPE, crashSignalHandler);
  std::signal(SIGILL, crashSignalHandler);
  std::signal(SIGSEGV, crashSignalHandler);
#endif
}
} // namespace stack_trace
