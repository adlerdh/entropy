#pragma once

#include "rendering/mesh/MeshExtraction.h"
#include "rendering/mesh/MeshExtractionRunner.h"

#include <cstddef>
#include <condition_variable>
#include <deque>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace rendering::mesh
{

/**
 * @brief CPU-only result produced by a background mesh extraction job
 *
 * The job result is intentionally separate from `MeshCache`. Background jobs must not mutate the cache directly; the
 * owner applies completed results on the application/render thread.
 */
struct MeshExtractionJobResult
{
  MeshGeometryKey key;                        //!< Geometry key requested by the owner
  std::optional<MeshExtractionResult> result; //!< Extracted mesh, or empty on failure
  bool empty = false;                         //!< Successful extraction with no contour for the requested value
  std::vector<std::string> diagnostics;       //!< Failure or warning messages
};

/**
 * @brief Background callable used by the mesh extraction queue
 */
using MeshExtractionJob = std::function<MeshExtractionJobResult()>;

/**
 * @brief Bounded single-worker CPU extraction scheduler with duplicate-key suppression
 *
 * The queue owns asynchronous CPU jobs only. It does not own OpenGL objects and does not mutate the mesh cache from
 * worker threads.
 */
class MeshExtractionQueue
{
public:
  /**
   * @brief Construct a queue with a conservative active-job limit
   * @param maxActiveJobs Maximum queued/running jobs. Values below one are clamped to one.
   */
  explicit MeshExtractionQueue(std::size_t maxActiveJobs = 64);
  ~MeshExtractionQueue();

  MeshExtractionQueue(const MeshExtractionQueue&) = delete;
  MeshExtractionQueue& operator=(const MeshExtractionQueue&) = delete;

  /**
   * @brief Queue one extraction job unless the key is already active
   * @param key Geometry key produced by the job
   * @param description Human-readable description of the mesh being computed
   * @param job CPU-only extraction job
   * @return True when a new job was queued
   * @throw Propagates allocation failures or `std::async` launch failures
   */
  bool submit(MeshGeometryKey key, std::string description, MeshExtractionJob job);

  /** Return whether a distinct job can be accepted before its expensive snapshot is constructed. */
  bool canSubmit(const MeshGeometryKey& key) const;

  /** Cancel queued work whose geometry is no longer represented by application state. */
  std::size_t cancelNotIn(const std::unordered_set<MeshGeometryKey, MeshGeometryKeyHash>& liveKeys);

  /**
   * @brief Move finished job results out of the queue
   * @return Completed results in submission order where possible
   * Exceptions thrown by jobs are converted to failed results with diagnostics.
   */
  std::vector<MeshExtractionJobResult> takeCompleted();

  /**
   * @brief Return whether a key is currently queued or running
   * @param key Geometry key to query
   * @return Whether the key is active
   */
  bool active(const MeshGeometryKey& key) const;

  /**
   * @brief Return the number of queued or running jobs
   * @return Active job count
   */
  std::size_t activeCount() const;

  /**
   * @brief Return descriptions for queued or running jobs
   * @return Active job descriptions in queue order
   * @throw Propagates allocation failures
   */
  std::vector<std::string> activeDescriptions() const;

  /**
   * @brief Return the maximum number of queued or running jobs
   * @return Active-job limit
   */
  std::size_t maxActiveJobs() const;

  /**
   * @brief Set the maximum number of queued or running jobs
   * @param maxActiveJobs Active-job limit. Values below one are clamped to one.
   */
  void setMaxActiveJobs(std::size_t maxActiveJobsArg);

private:
  struct ScheduledJob
  {
    MeshGeometryKey key;
    std::string description;
    MeshExtractionJob job;
  };

  void run();

  mutable std::mutex m_mutex;
  std::condition_variable_any m_workAvailable;
  std::deque<ScheduledJob> m_pendingJobs;
  std::deque<MeshExtractionJobResult> m_completedJobs;
  std::unordered_map<MeshGeometryKey, std::string, MeshGeometryKeyHash> m_activeJobs;
  std::optional<MeshGeometryKey> m_runningKey;
  std::size_t m_maxActiveJobs = 64;
  bool m_stopping = false;
  std::thread m_worker;
};

/**
 * @brief Apply a completed background extraction result to a pending cache entry
 * @param jobResult Result produced by a background job
 * @param cache Cache receiving the ready, failed, or stale state
 * @return Cache update summary
 * @throw Propagates allocation failures
 */
MeshExtractionRunResult applyExtractionJobResult(MeshExtractionJobResult jobResult, MeshCache& cache);

} // namespace rendering::mesh
