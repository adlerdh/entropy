#include "rendering/mesh/MeshExtractionService.h"

#include <algorithm>
#include <iterator>
#include <utility>

namespace rendering::mesh
{

MeshExtractionService::MeshExtractionService(const std::size_t maximumActiveJobs) : m_queue{maximumActiveJobs} {}

const MeshData* MeshExtractionService::readyMesh(const MeshGeometryKey& key) const noexcept
{
  return m_cache.readyMesh(key);
}

const MeshCacheEntry* MeshExtractionService::entry(const MeshGeometryKey& key) const noexcept
{
  return m_cache.find(key);
}

bool MeshExtractionService::needsExtraction(const MeshGeometryKey& key, const uint32_t maximumFailureCount)
  const noexcept
{
  if (m_cache.readyMesh(key)) {
    return false;
  }
  const MeshCacheEntry* cacheEntry = m_cache.find(key);
  return !cacheEntry || m_cache.canRetry(key, maximumFailureCount);
}

bool MeshExtractionService::canSubmit(const MeshGeometryKey& key, const uint32_t maximumFailureCount) const
{
  return needsExtraction(key, maximumFailureCount) && m_queue.canSubmit(key);
}

bool MeshExtractionService::submit(MeshGeometryKey key, std::string description, MeshExtractionJob job)
{
  const MeshCacheEntry* cacheEntry = m_cache.find(key);
  const uint32_t priorFailureCount = cacheEntry ? cacheEntry->failureCount : 0;
  if (!m_queue.submit(key, std::move(description), std::move(job))) {
    return false;
  }

  m_cache.markPending(std::move(key), priorFailureCount);
  return true;
}

std::vector<MeshExtractionRunResult> MeshExtractionService::consumeCompleted()
{
  std::vector<MeshExtractionJobResult> completed = m_queue.takeCompleted();
  std::vector<MeshExtractionRunResult> summaries;
  summaries.reserve(completed.size());
  std::transform(
    completed.begin(),
    completed.end(),
    std::back_inserter(summaries),
    [this](MeshExtractionJobResult& result) { return applyExtractionJobResult(std::move(result), m_cache); });
  return summaries;
}

std::size_t MeshExtractionService::retainOnly(const MeshGeometryKeySet& liveKeys)
{
  m_queue.cancelNotIn(liveKeys);
  return m_cache.retainOnly(liveKeys);
}

std::size_t MeshExtractionService::activeCount() const
{
  return m_queue.activeCount();
}

std::vector<std::string> MeshExtractionService::activeDescriptions() const
{
  return m_queue.activeDescriptions();
}

} // namespace rendering::mesh
