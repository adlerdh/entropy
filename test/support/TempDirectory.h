#pragma once

#include "common/UuidUtility.h"

#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>

namespace entropy::test
{
/// Own a uniquely named temporary directory, isolated from other fixtures and concurrent CTest processes.
/// Destruction recursively removes this directory and any fixture files placed in it.
class TempDirectory
{
public:
  /// Create a UUID-named directory under the system temporary directory; throw if creation fails.
  TempDirectory()
    : m_path(std::filesystem::temp_directory_path() / ("entropy-test-" + uuids::to_string(generateRandomUuid())))
  {
    if (!std::filesystem::create_directory(m_path)) throw std::runtime_error("Cannot create test directory");
  }

  /// Remove the owned directory and its contents, ignoring filesystem cleanup errors.
  ~TempDirectory()
  {
    std::error_code ignored;
    std::filesystem::remove_all(m_path, ignored);
  }

  TempDirectory(const TempDirectory&) = delete;            ///< Directory ownership cannot be copied.
  TempDirectory& operator=(const TempDirectory&) = delete; ///< Directory ownership cannot be reassigned by copying.

  /// Return the directory path for constructing fixture artifact paths.
  std::filesystem::path path() const
  {
    return m_path;
  }

private:
  std::filesystem::path m_path;
};

/// Read a file in binary mode into a string, preserving embedded null bytes.
/// @throws std::runtime_error If the input file cannot be opened.
inline std::string readBytes(const std::filesystem::path& path)
{
  std::ifstream stream(path, std::ios::binary);
  if (!stream) throw std::runtime_error("Cannot read test artifact: " + path.string());
  return {std::istreambuf_iterator<char>{stream}, std::istreambuf_iterator<char>{}};
}

/// Create or truncate a binary file and write all supplied bytes, closing it before returning.
/// @throws std::ios_base::failure If opening, writing, or closing the file fails.
inline void writeBytes(const std::filesystem::path& path, const std::string& bytes)
{
  std::ofstream stream(path, std::ios::binary);
  stream.exceptions(std::ios::badbit | std::ios::failbit);
  stream << bytes;
  stream.close();
}
} // namespace entropy::test
