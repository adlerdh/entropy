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
  /**
   * @brief Create an exception from a C string and source location.
   * @param msg Error message.
   * @param location Source location attached to the error.
   */
  Exception(const char* msg, const std::source_location& location) : std::runtime_error(msg)
  {
    m_msg = makeMessage(msg, location.file_name(), location.function_name(), static_cast<int>(location.line()));
  }

  /**
   * @brief Create an exception from a string and source location.
   * @param msg Error message.
   * @param location Source location attached to the error.
   */
  Exception(const std::string& msg, const std::source_location& location) : Exception(msg.c_str(), location) {}

  /**
   * @brief Create an exception from explicit source-location fields.
   * @param msg Error message.
   * @param file Source file name.
   * @param function Source function name.
   * @param line Source line number.
   */
  Exception(const char* msg, const char* file, const char* function, int line) : std::runtime_error(msg)
  {
    m_msg = makeMessage(msg, file, function, line);
  }

  /**
   * @brief Create an exception from a string and explicit source-location fields.
   * @param msg Error message.
   * @param file Source file name.
   * @param function Source function name.
   * @param line Source line number.
   */
  Exception(const std::string& msg, const char* file, const char* function, int line)
    : Exception(msg.c_str(), file, function, line)
  {
  }

  /**
   * @brief Destroy the exception.
   */
  virtual ~Exception() = default;

  /**
   * @brief Return the formatted error message.
   * @return Error message including source-location context.
   */
  const char* what() const noexcept
  {
    return m_msg.c_str();
  }

private:
  /**
   * @brief Format an error message with source-location context.
   * @param msg Error message.
   * @param file Source file name.
   * @param function Source function name.
   * @param line Source line number.
   * @return Formatted error message.
   */
  static std::string makeMessage(const char* msg, const char* file, const char* function, int line)
  {
    std::ostringstream ss;
    ss << "[in function '" << function << "'; file '" << file << "' : line " << line << "] " << msg;
    return ss.str();
  }

  std::string m_msg;
};

/**
 * @brief Throw an Exception with the current C++ source location.
 * @param message Error message.
 * @param location Source location attached to the error.
 */
[[noreturn]] inline void throwDebug(
  const std::string& message,
  const std::source_location& location = std::source_location::current())
{
  throw Exception(message, location);
}

/**
 * @brief Throw an Exception with explicit source-location fields.
 * @param message Error message.
 * @param file Source file name.
 * @param function Source function name.
 * @param line Source line number.
 */
[[noreturn]] inline void throwDebug(const std::string& message, const char* file, const char* function, int line)
{
  throw Exception(message, file, function, line);
}

/**
 * @brief Throw an Exception with explicit source-location fields.
 * @param message Error message.
 * @param file Source file name.
 * @param function Source function name.
 * @param line Source line number.
 */
[[noreturn]] inline void throwDebug(const char* message, const char* file, const char* function, int line)
{
  throw Exception(message, file, function, line);
}

/**
 * @brief Throw an Exception with the current C++ source location.
 * @param message Error message.
 * @param location Source location attached to the error.
 */
[[noreturn]] inline void throwDebug(
  const char* message,
  const std::source_location& location = std::source_location::current())
{
  throw Exception(message, location);
}
