#pragma once

#include "registration/Execution.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace registration
{

/**
 * @brief UI-facing state for one registration job.
 */
struct JobRecord
{
  std::string id;                         //!< Stable job identifier.
  JobSpec spec;                           //!< Job request.
  JobStatus status = JobStatus::Queued;   //!< Current normalized status.
  std::vector<ProgressEvent> progress;    //!< Parsed progress events.
  std::vector<std::string> warnings;      //!< Non-fatal warnings.
  std::string errorMessage;               //!< Failure summary.
  std::optional<ResultManifest> manifest; //!< Imported or completed result manifest.
};

/**
 * @brief In-memory registration job list used by UI and app controllers.
 */
class JobStore
{
public:
  /**
   * @brief Add a job to the store.
   * @param spec Job specification.
   * @return Created job ID.
   */
  std::string add(JobSpec spec);

  /**
   * @brief Return all jobs in insertion order.
   * @return Stored job records.
   */
  [[nodiscard]] const std::vector<JobRecord>& jobs() const;

  /**
   * @brief Find a mutable job record by ID.
   * @param id Job ID.
   * @return Matching record, or nullptr.
   */
  JobRecord* find(const std::string& id);

  /**
   * @brief Find an immutable job record by ID.
   * @param id Job ID.
   * @return Matching record, or nullptr.
   */
  [[nodiscard]] const JobRecord* find(const std::string& id) const;

  /**
   * @brief Set one job's status.
   * @param id Job ID.
   * @param status New status.
   * @return True iff the job exists.
   */
  bool setStatus(const std::string& id, JobStatus status);

  /**
   * @brief Append a parsed progress event to one job.
   * @param id Job ID.
   * @param event Progress event.
   * @return True iff the job exists.
   */
  bool appendProgress(const std::string& id, ProgressEvent event);

  /**
   * @brief Complete a job from an execution summary.
   * @param id Job ID.
   * @param execution Execution summary.
   * @return True iff the job exists.
   */
  bool applyExecution(const std::string& id, const JobExecution& execution);

  /**
   * @brief Return whether at least one job is active.
   * @return True when a job is queued, preparing, running, writing, or importing.
   */
  [[nodiscard]] bool hasActiveJobs() const;

private:
  std::vector<JobRecord> m_jobs;
  std::uint64_t m_nextId = 1;
};

/**
 * @brief Return the latest normalized progress for a job.
 * @param job Job to inspect.
 * @return Latest progress value in [0, 1], or std::nullopt when unavailable.
 */
std::optional<double> latestProgress(const JobRecord& job);

/**
 * @brief Return the latest human-readable status/progress message for a job.
 * @param job Job to inspect.
 * @return Latest message text, or an empty string when unavailable.
 */
std::string latestMessage(const JobRecord& job);

/**
 * @brief Return whether a status represents an active job.
 * @param status Status to inspect.
 * @return True for queued, preparing, running, writing, or importing jobs.
 */
bool isActiveJobStatus(JobStatus status);

} // namespace registration
