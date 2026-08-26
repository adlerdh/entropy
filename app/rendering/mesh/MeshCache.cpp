#include "rendering/mesh/MeshCache.h"

#include <utility>

namespace rendering::mesh
{

void MeshCache::markPending(MeshGeometryKey key, const uint32_t priorFailureCount)
{
  m_entries.insert_or_assign(
    std::move(key),
    MeshCacheEntry{
      .state = MeshCacheState::Pending,
      .mesh = std::nullopt,
      .diagnostics = {},
      .failureCount = priorFailureCount});
}

bool MeshCache::canRetry(const MeshGeometryKey& key, const uint32_t maximumFailureCount) const noexcept
{
  const MeshCacheEntry* entry = find(key);
  return entry && entry->state == MeshCacheState::Failed && entry->failureCount < maximumFailureCount;
}

void MeshCache::storeReady(MeshExtractionResult result)
{
  MeshCacheEntry entry{
    .state = MeshCacheState::Ready,
    .mesh = std::move(result.mesh),
    .diagnostics = std::move(result.diagnostics)};
  m_entries.insert_or_assign(std::move(result.key), std::move(entry));
}

bool MeshCache::storeReadyIfPending(MeshExtractionResult result)
{
  const auto it = m_entries.find(result.key);
  if (it == m_entries.end() || it->second.state != MeshCacheState::Pending) {
    return false;
  }

  MeshCacheEntry entry{
    .state = MeshCacheState::Ready,
    .mesh = std::move(result.mesh),
    .diagnostics = std::move(result.diagnostics)};
  it->second = std::move(entry);
  return true;
}

bool MeshCache::storeEmptyIfPending(const MeshGeometryKey& key, std::vector<std::string> diagnostics)
{
  const auto it = m_entries.find(key);
  if (it == m_entries.end() || it->second.state != MeshCacheState::Pending) {
    return false;
  }
  it->second =
    MeshCacheEntry{.state = MeshCacheState::Empty, .mesh = std::nullopt, .diagnostics = std::move(diagnostics)};
  return true;
}

void MeshCache::storeFailed(MeshGeometryKey key, std::vector<std::string> diagnostics)
{
  MeshCacheEntry entry{
    .state = MeshCacheState::Failed,
    .mesh = std::nullopt,
    .diagnostics = std::move(diagnostics),
    .failureCount = 1};
  m_entries.insert_or_assign(std::move(key), std::move(entry));
}

bool MeshCache::storeFailedIfPending(const MeshGeometryKey& key, std::vector<std::string> diagnostics)
{
  const auto it = m_entries.find(key);
  if (it == m_entries.end() || it->second.state != MeshCacheState::Pending) {
    return false;
  }

  it->second = MeshCacheEntry{
    .state = MeshCacheState::Failed,
    .mesh = std::nullopt,
    .diagnostics = std::move(diagnostics),
    .failureCount = it->second.failureCount + 1};
  return true;
}

std::size_t MeshCache::markSourceStale(const uuids::uuid& sourceUid)
{
  std::size_t count = 0;
  for (auto& [key, entry] : m_entries) {
    if (key.sourceUid != sourceUid || entry.state == MeshCacheState::Evicted) {
      continue;
    }

    entry.state = MeshCacheState::Stale;
    ++count;
  }

  return count;
}

std::size_t MeshCache::evictSource(const uuids::uuid& sourceUid)
{
  std::size_t count = 0;
  for (auto& [key, entry] : m_entries) {
    if (key.sourceUid != sourceUid) {
      continue;
    }

    entry.state = MeshCacheState::Evicted;
    entry.mesh.reset();
    ++count;
  }

  return count;
}

bool MeshCache::erase(const MeshGeometryKey& key) noexcept
{
  return m_entries.erase(key) > 0u;
}

const MeshCacheEntry* MeshCache::find(const MeshGeometryKey& key) const noexcept
{
  const auto it = m_entries.find(key);
  return it == m_entries.end() ? nullptr : &it->second;
}

const MeshData* MeshCache::readyMesh(const MeshGeometryKey& key) const noexcept
{
  const MeshCacheEntry* entry = find(key);
  if (!entry || entry->state != MeshCacheState::Ready || !entry->mesh) {
    return nullptr;
  }

  return &*entry->mesh;
}

bool MeshCache::contains(const MeshGeometryKey& key) const noexcept
{
  return m_entries.contains(key);
}

std::size_t MeshCache::size() const noexcept
{
  return m_entries.size();
}

void MeshCache::clear() noexcept
{
  m_entries.clear();
}

} // namespace rendering::mesh
