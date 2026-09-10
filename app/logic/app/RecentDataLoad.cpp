#include "logic/app/RecentDataLoad.h"

#include <utility>

namespace recent_data
{
void PendingLoad::begin(Kind entryKind, std::vector<std::filesystem::path> paths)
{
  m_entry = Entry{entryKind, std::move(paths)};
}

void PendingLoad::appendPath(std::filesystem::path path)
{
  if (m_entry) {
    m_entry->paths.push_back(std::move(path));
  }
}

void PendingLoad::replacePaths(std::vector<std::filesystem::path> paths)
{
  if (m_entry) {
    m_entry->paths = std::move(paths);
  }
}

void PendingLoad::cancel()
{
  m_entry = std::nullopt;
}

std::optional<Entry> PendingLoad::takeCompleted()
{
  if (!m_entry || m_entry->paths.empty()) {
    cancel();
    return std::nullopt;
  }

  std::optional<Entry> completed = std::move(m_entry);
  m_entry = std::nullopt;
  return completed;
}

std::optional<Kind> PendingLoad::kind() const
{
  return m_entry ? std::optional<Kind>{m_entry->kind} : std::nullopt;
}
} // namespace recent_data
