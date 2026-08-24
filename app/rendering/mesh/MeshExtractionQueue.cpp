#include "rendering/mesh/MeshExtractionQueue.h"

#include <algorithm>
#include <chrono>
#include <functional>
#include <exception>
#include <string>
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
    diagnostics.emplace_back("Extractor returned no mesh");
  }

  const bool accepted = cache.storeFailedIfPending(key, diagnostics);
  return MeshExtractionRunResult{
    .key = key,
    .status = accepted ? MeshExtractionRunStatus::Failed : MeshExtractionRunStatus::Stale,
    .diagnostics = std::move(diagnostics)};
}

} // namespace

MeshExtractionQueue::MeshExtractionQueue(const std::size_t maxActiveJobs)
  : m_maxActiveJobs(std::max<std::size_t>(1, maxActiveJobs))
{
}

bool MeshExtractionQueue::submit(MeshGeometryKey key, std::string description, MeshExtractionJob job)
{
  if (active(key) || m_activeJobs.size() >= m_maxActiveJobs) {
    return false;
  }

  const MeshGeometryKey jobKey = key;
  auto guardedJob = [jobKey, job = std::move(job)]() mutable {
    try {
      return job();
    }
    catch (const std::exception& e) {
      return MeshExtractionJobResult{
        .key = jobKey,
        .result = std::nullopt,
        .diagnostics = {std::string{"Mesh extraction threw an exception: "} + e.what()}};
    }
    catch (...) {
      return MeshExtractionJobResult{
        .key = jobKey,
        .result = std::nullopt,
        .diagnostics = {std::string{"Mesh extraction threw an unknown exception"}}};
    }
  };

  m_activeJobs.push_back(ActiveJob{
    .key = std::move(key),
    .description = std::move(description),
    .future = std::async(std::launch::async, std::move(guardedJob))});
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

    try {
      completed.push_back(it->future.get());
    }
    catch (const std::exception& e) {
      completed.push_back(MeshExtractionJobResult{
        .key = it->key,
        .result = std::nullopt,
        .diagnostics = {std::string{"Unable to collect mesh extraction result: "} + e.what()}});
    }
    catch (...) {
      completed.push_back(MeshExtractionJobResult{
        .key = it->key,
        .result = std::nullopt,
        .diagnostics = {std::string{"Unable to collect mesh extraction result: unknown exception"}}});
    }
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

std::vector<std::string> MeshExtractionQueue::activeDescriptions() const
{
  std::vector<std::string> descriptions;
  descriptions.reserve(m_activeJobs.size());
  for (const ActiveJob& job : m_activeJobs) {
    descriptions.push_back(job.description);
  }
  return descriptions;
}

std::size_t MeshExtractionQueue::maxActiveJobs() const noexcept
{
  return m_maxActiveJobs;
}

void MeshExtractionQueue::setMaxActiveJobs(const std::size_t maxActiveJobs) noexcept
{
  m_maxActiveJobs = std::max<std::size_t>(1, maxActiveJobs);
}

MeshExtractionRunResult applyExtractionJobResult(MeshExtractionJobResult jobResult, MeshCache& cache)
{
  if (!jobResult.result) {
    return failedOrStale(jobResult.key, std::move(jobResult.diagnostics), cache);
  }

  if (jobResult.result->key != jobResult.key) {
    std::vector<std::string> diagnostics = std::move(jobResult.diagnostics);
    diagnostics.emplace_back("Extractor returned a mesh for a different geometry key");
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
