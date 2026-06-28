#include "registration/Jobs.h"

#include <algorithm>
#include <sstream>

namespace registration
{
namespace
{

bool isActive(JobStatus status)
{
  return status == JobStatus::Queued || status == JobStatus::PreparingInputs || status == JobStatus::Running ||
         status == JobStatus::WritingOutputs || status == JobStatus::ImportingOutputs;
}

void applyProgressStatus(JobRecord& job, const ProgressEvent& event)
{
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
}

} // namespace

std::string JobStore::add(JobSpec spec)
{
  std::ostringstream id;
  id << "registration-" << m_nextId++;

  JobRecord record;
  record.id = id.str();
  record.spec = std::move(spec);
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
  job->progress.insert(job->progress.end(), execution.progressEvents.begin(), execution.progressEvents.end());
  job->warnings.insert(job->warnings.end(), execution.warnings.begin(), execution.warnings.end());
  job->errorMessage = execution.errorMessage;
  return true;
}

bool JobStore::hasActiveJobs() const
{
  return std::any_of(m_jobs.begin(), m_jobs.end(), [](const JobRecord& job) { return isActive(job.status); });
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

} // namespace registration
