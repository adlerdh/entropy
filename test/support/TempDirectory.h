#pragma once

#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include "common/UuidUtility.h"

namespace entropy::test
{
// Unique per fixture, including concurrent CTest processes. Never removes user data.
class TempDirectory
{
public:
  TempDirectory()
    : m_path(std::filesystem::temp_directory_path() / ("entropy-test-" + uuids::to_string(generateRandomUuid())))
  {
    if (!std::filesystem::create_directory(m_path)) throw std::runtime_error("Cannot create test directory");
  }
  ~TempDirectory()
  {
    std::error_code ignored;
    std::filesystem::remove_all(m_path, ignored);
  }
  TempDirectory(const TempDirectory&) = delete;
  TempDirectory& operator=(const TempDirectory&) = delete;
  std::filesystem::path path() const
  {
    return m_path;
  }

private:
  std::filesystem::path m_path;
};

inline std::string readBytes(const std::filesystem::path& path)
{
  std::ifstream stream(path, std::ios::binary);
  if (!stream) throw std::runtime_error("Cannot read test artifact: " + path.string());
  return {std::istreambuf_iterator<char>{stream}, std::istreambuf_iterator<char>{}};
}

inline void writeBytes(const std::filesystem::path& path, const std::string& bytes)
{
  std::ofstream stream(path, std::ios::binary);
  stream.exceptions(std::ios::badbit | std::ios::failbit);
  stream << bytes;
  stream.close();
}
} // namespace entropy::test
