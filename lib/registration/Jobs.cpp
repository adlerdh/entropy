#include "registration/Jobs.h"

#include <algorithm>
#include <filesystem>
#include <iomanip>
#include <sstream>

namespace registration
{
bool isActiveJobStatus(JobStatus status)
{
  return status == JobStatus::Queued || status == JobStatus::PreparingInputs || status == JobStatus::Running ||
         status == JobStatus::WritingOutputs || status == JobStatus::ImportingOutputs;
}

namespace
{

bool isTerminalJobStatus(JobStatus status)
{
  return status == JobStatus::Completed || status == JobStatus::Cancelled || status == JobStatus::Failed;
}

void updateTimingForStatus(JobRecord& job, JobStatus status)
{
  const auto now = JobRecord::Clock::now();
  if (!job.startedAt && status == JobStatus::Running) {
    job.startedAt = now;
  }
  if (isTerminalJobStatus(status) && !job.endedAt) {
    if (!job.startedAt) {
      job.startedAt = job.queuedAt;
    }
    job.endedAt = now;
  }
}

void applyProgressStatus(JobRecord& job, const ProgressEvent& event)
{
  const JobStatus oldStatus = job.status;
  switch (event.kind) {
    case ProgressEventKind::Started:
      job.status = JobStatus::Running;
      break;
    case ProgressEventKind::Completed:
      job.status = JobStatus::Completed;
      break;
    case ProgressEventKind::Cancelled:
      job.status = JobStatus::Cancelled;
      break;
    case ProgressEventKind::Failed:
      job.status = JobStatus::Failed;
      break;
    case ProgressEventKind::Progress:
    case ProgressEventKind::StageStarted:
    case ProgressEventKind::Artifact:
    case ProgressEventKind::Warning:
      break;
  }
  if (job.status != oldStatus) {
    updateTimingForStatus(job, job.status);
  }
}

} // namespace

std::string JobStore::add(JobSpec spec)
{
  std::ostringstream id;
  id << "registration-" << m_nextId++;
  const std::string jobId = id.str();

  if (spec.outputDirectory == std::filesystem::temp_directory_path()) {
    spec.outputDirectory = std::filesystem::temp_directory_path() / "entropy-registration" / jobId;
  }

  JobRecord record;
  record.id = jobId;
  record.order = m_nextId - 1;
  record.spec = std::move(spec);
  record.queuedAt = JobRecord::Clock::now();
  m_jobs.push_back(std::move(record));
  return m_jobs.back().id;
}

const std::vector<JobRecord>& JobStore::jobs() const
{
  return m_jobs;
}

JobRecord* JobStore::find(const std::string& id)
{
  const auto it = std::find_if(m_jobs.begin(), m_jobs.end(), [&](const JobRecord& job) { return job.id == id; });
  return it == m_jobs.end() ? nullptr : &*it;
}

const JobRecord* JobStore::find(const std::string& id) const
{
  const auto it = std::find_if(m_jobs.begin(), m_jobs.end(), [&](const JobRecord& job) { return job.id == id; });
  return it == m_jobs.end() ? nullptr : &*it;
}

bool JobStore::setStatus(const std::string& id, JobStatus status)
{
  JobRecord* job = find(id);
  if (!job) {
    return false;
  }
  job->status = status;
  updateTimingForStatus(*job, status);
  return true;
}

bool JobStore::appendProgress(const std::string& id, ProgressEvent event)
{
  JobRecord* job = find(id);
  if (!job) {
    return false;
  }
  applyProgressStatus(*job, event);
  if (event.kind == ProgressEventKind::Warning && !event.message.empty()) {
    job->warnings.push_back(event.message);
  }
  if (event.kind == ProgressEventKind::Failed && !event.message.empty()) {
    job->errorMessage = event.message;
    job->status = JobStatus::Failed;
  }
  job->progress.push_back(std::move(event));
  return true;
}

bool JobStore::applyExecution(const std::string& id, const JobExecution& execution)
{
  JobRecord* job = find(id);
  if (!job) {
    return false;
  }
  job->status = execution.status;
  updateTimingForStatus(*job, execution.status);
  job->commands.insert(job->commands.end(), execution.commands.begin(), execution.commands.end());
  job->progress.insert(job->progress.end(), execution.progressEvents.begin(), execution.progressEvents.end());
  job->outputLines.insert(job->outputLines.end(), execution.outputLines.begin(), execution.outputLines.end());
  job->warnings.insert(job->warnings.end(), execution.warnings.begin(), execution.warnings.end());
  job->manifest = execution.manifest;
  job->errorMessage = execution.errorMessage;
  return true;
}

bool JobStore::hasActiveJobs() const
{
  return std::any_of(m_jobs.begin(), m_jobs.end(), [](const JobRecord& job) { return isActiveJobStatus(job.status); });
}

std::optional<double> latestProgress(const JobRecord& job)
{
  for (auto it = job.progress.rbegin(); it != job.progress.rend(); ++it) {
    if (it->progress) {
      return it->progress;
    }
  }
  return std::nullopt;
}

std::string latestMessage(const JobRecord& job)
{
  if ((job.status == JobStatus::Failed || job.status == JobStatus::Cancelled) && !job.errorMessage.empty()) {
    return job.errorMessage;
  }
  for (auto it = job.progress.rbegin(); it != job.progress.rend(); ++it) {
    if (!it->message.empty()) {
      return it->message;
    }
  }
  if (!job.errorMessage.empty()) {
    return job.errorMessage;
  }
  return {};
}

std::optional<std::chrono::system_clock::duration> jobDuration(const JobRecord& job)
{
  if (!job.startedAt) {
    return std::nullopt;
  }

  const auto end = job.endedAt.value_or(JobRecord::Clock::now());
  if (end < *job.startedAt) {
    return std::chrono::system_clock::duration::zero();
  }
  return end - *job.startedAt;
}

std::string formatDuration(std::chrono::system_clock::duration duration)
{
  using namespace std::chrono;

  const auto millis = duration_cast<milliseconds>(duration).count();
  std::ostringstream stream;
  if (millis < 1000) {
    stream << millis << " ms";
    return stream.str();
  }

  const double seconds = static_cast<double>(millis) / 1000.0;
  if (seconds < 60.0) {
    stream << std::fixed << std::setprecision(seconds < 10.0 ? 1 : 0) << seconds << " sec";
    return stream.str();
  }

  const double minutes = seconds / 60.0;
  if (minutes < 60.0) {
    stream << std::fixed << std::setprecision(minutes < 10.0 ? 1 : 0) << minutes << " min";
    return stream.str();
  }

  const double hours = minutes / 60.0;
  stream << std::fixed << std::setprecision(hours < 10.0 ? 1 : 0) << hours << " hr";
  return stream.str();
}

} // namespace registration
