#pragma once

#include <sstream>
#include <source_location>
#include <stdexcept>
#include <string>

/**
 * @brief A friendly wrapper around \c std::runtime_error that prints the file name,
 * function name, and line number on which the exception occurred.
 */
class Exception : public std::runtime_error
{
public:
  Exception(const char* msg, const std::source_location& location) : std::runtime_error(msg)
  {
    m_msg = makeMessage(msg, location.file_name(), location.function_name(), static_cast<int>(location.line()));
  }

  Exception(const std::string& msg, const std::source_location& location) : Exception(msg.c_str(), location) {}

  Exception(const char* msg, const char* file, const char* function, int line) : std::runtime_error(msg)
  {
    m_msg = makeMessage(msg, file, function, line);
  }

  Exception(const std::string& msg, const char* file, const char* function, int line)
    : Exception(msg.c_str(), file, function, line)
  {
  }

  virtual ~Exception() = default;

  const char* what() const noexcept
  {
    return m_msg.c_str();
  }

private:
  static std::string makeMessage(const char* msg, const char* file, const char* function, int line)
  {
    std::ostringstream ss;
    ss << "[in function '" << function << "'; file '" << file << "' : line " << line << "] " << msg;
    return ss.str();
  }

  std::string m_msg;
};

[[noreturn]] inline void throwDebug(
  const std::string& message,
  const std::source_location& location = std::source_location::current())
{
  throw Exception(message, location);
}

[[noreturn]] inline void throwDebug(const std::string& message, const char* file, const char* function, int line)
{
  throw Exception(message, file, function, line);
}

[[noreturn]] inline void throwDebug(const char* message, const char* file, const char* function, int line)
{
  throw Exception(message, file, function, line);
}

[[noreturn]] inline void throwDebug(
  const char* message,
  const std::source_location& location = std::source_location::current())
{
  throw Exception(message, location);
}
