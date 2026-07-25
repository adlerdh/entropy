#include "rendering/mesh/MeshExtractionQueue.h"

#include <algorithm>
#include <chrono>
#include <utility>

namespace rendering::mesh
{

namespace
{

bool ready(std::future<MeshExtractionJobResult>& future)
{
  return future.wait_for(std::chrono::seconds{0}) == std::future_status::ready;
}

MeshExtractionRunResult
failedOrStale(const MeshGeometryKey& key, std::vector<std::string> diagnostics, MeshCache& cache)
{
  if (diagnostics.empty()) {
    diagnostics.push_back("Extractor returned no mesh");
  }

  const bool accepted = cache.storeFailedIfPending(key, diagnostics);
  return MeshExtractionRunResult{
    .key = key,
    .status = accepted ? MeshExtractionRunStatus::Failed : MeshExtractionRunStatus::Stale,
    .diagnostics = std::move(diagnostics)};
}

} // namespace

bool MeshExtractionQueue::submit(MeshGeometryKey key, MeshExtractionJob job)
{
  if (active(key)) {
    return false;
  }

  m_activeJobs.push_back(ActiveJob{.key = std::move(key), .future = std::async(std::launch::async, std::move(job))});
  return true;
}

std::vector<MeshExtractionJobResult> MeshExtractionQueue::takeCompleted()
{
  std::vector<MeshExtractionJobResult> completed;

  for (auto it = m_activeJobs.begin(); it != m_activeJobs.end();) {
    if (!ready(it->future)) {
      ++it;
      continue;
    }

    completed.push_back(it->future.get());
    it = m_activeJobs.erase(it);
  }

  return completed;
}

bool MeshExtractionQueue::active(const MeshGeometryKey& key) const
{
  return std::any_of(m_activeJobs.begin(), m_activeJobs.end(), [&key](const ActiveJob& job) { return job.key == key; });
}

std::size_t MeshExtractionQueue::activeCount() const noexcept
{
  return m_activeJobs.size();
}

MeshExtractionRunResult applyExtractionJobResult(MeshExtractionJobResult jobResult, MeshCache& cache)
{
  if (!jobResult.result) {
    return failedOrStale(jobResult.key, std::move(jobResult.diagnostics), cache);
  }

  if (jobResult.result->key != jobResult.key) {
    std::vector<std::string> diagnostics = std::move(jobResult.diagnostics);
    diagnostics.push_back("Extractor returned a mesh for a different geometry key");
    return failedOrStale(jobResult.key, std::move(diagnostics), cache);
  }

  const std::vector<std::string> diagnostics = jobResult.result->diagnostics;
  const MeshGeometryKey key = jobResult.key;
  const bool accepted = cache.storeReadyIfPending(std::move(*jobResult.result));
  return MeshExtractionRunResult{
    .key = key,
    .status = accepted ? MeshExtractionRunStatus::Ready : MeshExtractionRunStatus::Stale,
    .diagnostics = diagnostics};
}

} // namespace rendering::mesh
