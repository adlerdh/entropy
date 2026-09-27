#pragma once

#include "registration/Jobs.h"
#include <algorithm>
#include <cstddef>
#include <functional>

namespace registration
{
/**
 * @brief Propagate cancellations and dispatch queued jobs up to the concurrency limit on the owner thread.
 * @param jobs Jobs in dispatch order; callbacks must not invalidate this collection during iteration.
 * @param running Number of jobs already running.
 * @param maximum Concurrency limit, clamped to at least one.
 * @param isRunning Return whether a job ID already has an active worker.
 * @param cancel Request cancellation for each job marked Cancelled, even when all slots are occupied.
 * @param prepare Prepare and dispatch a queued job; return true only when it consumes a running slot.
 * @details Failed preparations do not consume capacity. Callbacks own process and filesystem operations.
 */
inline void dispatchQueuedJobs(
  const std::vector<JobRecord>& jobs,
  std::size_t running,
  std::size_t maximum,
  const std::function<bool(const std::string&)>& isRunning,
  const std::function<void(const std::string&)>& cancel,
  const std::function<bool(const JobRecord&)>& prepare)
{
  for (const auto& job : jobs)
    if (job.status == JobStatus::Cancelled) cancel(job.id);
  maximum = std::max(std::size_t{1}, maximum);
  if (running >= maximum) return;
  for (const auto& job : jobs) {
    if (job.status != JobStatus::Queued || isRunning(job.id)) continue;
    if (prepare(job) && ++running >= maximum) break;
  }
}
} // namespace registration
