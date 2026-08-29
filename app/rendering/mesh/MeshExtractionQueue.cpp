#include "rendering/mesh/MeshExtractionQueue.h"

#include <algorithm>
#include <exception>
#include <string>
#include <unordered_set>
#include <utility>

namespace rendering::mesh
{

namespace
{

MeshExtractionJobResult executeGuarded(const MeshGeometryKey& key, const MeshExtractionJob& job)
{
  try {
    return job();
  }
  catch (const std::exception& e) {
    return MeshExtractionJobResult{
      .key = key,
      .result = std::nullopt,
      .diagnostics = {std::string{"Mesh extraction threw an exception: "} + e.what()}};
  }
  catch (...) {
    return MeshExtractionJobResult{
      .key = key,
      .result = std::nullopt,
      .diagnostics = {std::string{"Mesh extraction threw an unknown exception"}}};
  }
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
  : m_maxActiveJobs(std::max<std::size_t>(1, maxActiveJobs)), m_worker([this] { run(); })
{
}

MeshExtractionQueue::~MeshExtractionQueue()
{
  {
    std::scoped_lock lock(m_mutex);
    m_stopping = true;
  }
  m_workAvailable.notify_all();
  if (m_worker.joinable()) {
    m_worker.join();
  }
}

bool MeshExtractionQueue::submit(MeshGeometryKey key, std::string description, MeshExtractionJob job)
{
  std::scoped_lock lock(m_mutex);
  if (m_activeJobs.contains(key) || m_activeJobs.size() >= m_maxActiveJobs) {
    return false;
  }

  m_activeJobs.emplace(key, description);
  m_pendingJobs.push_back(
    ScheduledJob{.key = std::move(key), .description = std::move(description), .job = std::move(job)});
  m_workAvailable.notify_one();
  return true;
}

bool MeshExtractionQueue::canSubmit(const MeshGeometryKey& key) const
{
  std::scoped_lock lock(m_mutex);
  return !m_activeJobs.contains(key) && m_activeJobs.size() < m_maxActiveJobs;
}

std::size_t MeshExtractionQueue::cancelNotIn(const std::unordered_set<MeshGeometryKey, MeshGeometryKeyHash>& liveKeys)
{
  std::scoped_lock lock(m_mutex);
  std::size_t cancelled = 0;
  for (auto it = m_pendingJobs.begin(); it != m_pendingJobs.end();) {
    if (liveKeys.contains(it->key)) {
      ++it;
      continue;
    }
    m_activeJobs.erase(it->key);
    it = m_pendingJobs.erase(it);
    ++cancelled;
  }
  return cancelled;
}

std::vector<MeshExtractionJobResult> MeshExtractionQueue::takeCompleted()
{
  std::scoped_lock lock(m_mutex);
  std::vector<MeshExtractionJobResult> completed;
  completed.reserve(m_completedJobs.size());
  while (!m_completedJobs.empty()) {
    m_activeJobs.erase(m_completedJobs.front().key);
    completed.push_back(std::move(m_completedJobs.front()));
    m_completedJobs.pop_front();
  }
  return completed;
}

bool MeshExtractionQueue::active(const MeshGeometryKey& key) const
{
  std::scoped_lock lock(m_mutex);
  return m_activeJobs.contains(key);
}

std::size_t MeshExtractionQueue::activeCount() const
{
  std::scoped_lock lock(m_mutex);
  return m_activeJobs.size();
}

std::vector<std::string> MeshExtractionQueue::activeDescriptions() const
{
  std::scoped_lock lock(m_mutex);
  std::vector<std::string> descriptions;
  descriptions.reserve(m_activeJobs.size());
  for (const auto& [key, description] : m_activeJobs) {
    static_cast<void>(key);
    descriptions.push_back(description);
  }
  return descriptions;
}

std::size_t MeshExtractionQueue::maxActiveJobs() const
{
  std::scoped_lock lock(m_mutex);
  return m_maxActiveJobs;
}

void MeshExtractionQueue::setMaxActiveJobs(const std::size_t maxActiveJobs)
{
  std::scoped_lock lock(m_mutex);
  m_maxActiveJobs = std::max<std::size_t>(1, maxActiveJobs);
}

void MeshExtractionQueue::run()
{
  while (true) {
    ScheduledJob scheduled;
    {
      std::unique_lock lock(m_mutex);
      m_workAvailable.wait(lock, [this] { return m_stopping || !m_pendingJobs.empty(); });
      if (m_stopping) {
        return;
      }
      scheduled = std::move(m_pendingJobs.front());
      m_pendingJobs.pop_front();
      m_runningKey = scheduled.key;
    }

    MeshExtractionJobResult result = executeGuarded(scheduled.key, scheduled.job);
    {
      std::scoped_lock lock(m_mutex);
      m_runningKey.reset();
      m_completedJobs.push_back(std::move(result));
    }
  }
}

MeshExtractionRunResult applyExtractionJobResult(MeshExtractionJobResult jobResult, MeshCache& cache)
{
  if (!jobResult.result) {
    if (jobResult.empty) {
      const bool accepted = cache.storeEmptyIfPending(jobResult.key, jobResult.diagnostics);
      return MeshExtractionRunResult{
        .key = jobResult.key,
        .status = accepted ? MeshExtractionRunStatus::Empty : MeshExtractionRunStatus::Stale,
        .diagnostics = std::move(jobResult.diagnostics)};
    }
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
