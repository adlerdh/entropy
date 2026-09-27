#pragma once

#include "registration/Jobs.h"
#include <algorithm>
#include <cstddef>
#include <functional>

namespace registration
{
// Called on the owner thread. Propagate cancellation before considering free
// slots; preparation failures do not consume capacity. Callbacks keep process
// ownership and filesystem boundaries outside the scheduling policy.
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
