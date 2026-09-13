#pragma once

#include <filesystem>
#include <optional>
#include <vector>

namespace recent_data
{
enum class Kind
{
  Images,
  Dicom,
  Project
};

struct Entry
{
  Kind kind = Kind::Images;
  std::vector<std::filesystem::path> paths;
};

/// Hold a Recent-data candidate until its load completes successfully.
class PendingLoad
{
public:
  void begin(Kind entryKind, std::vector<std::filesystem::path> paths);
  void appendPath(std::filesystem::path path);
  void replacePaths(std::vector<std::filesystem::path> paths);
  void cancel();

  [[nodiscard]] std::optional<Entry> takeCompleted();
  [[nodiscard]] std::optional<Kind> kind() const;

private:
  std::optional<Entry> m_entry;
};
} // namespace recent_data
