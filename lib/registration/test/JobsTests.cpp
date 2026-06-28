#include "registration/Jobs.h"

#include <catch2/catch_test_macros.hpp>

namespace
{

registration::JobSpec makeJob()
{
  registration::JobSpec job;
  job.fixedImage.uid = "fixed";
  job.movingImage.uid = "moving";
  return job;
}

registration::ProgressEvent progress(double value, std::string message = {})
{
  registration::ProgressEvent event;
  event.kind = registration::ProgressEventKind::Progress;
  event.progress = value;
  event.message = std::move(message);
  return event;
}

registration::ProgressEvent eventOfKind(registration::ProgressEventKind kind, std::string message = {})
{
  registration::ProgressEvent event;
  event.kind = kind;
  event.message = std::move(message);
  return event;
}

} // namespace

TEST_CASE("registration job store creates stable ordered job ids", "[registration][jobs]")
{
  registration::JobStore store;

  const std::string first = store.add(makeJob());
  const std::string second = store.add(makeJob());

  CHECK(first == "registration-1");
  CHECK(second == "registration-2");
  REQUIRE(store.jobs().size() == 2);
  CHECK(store.jobs().front().id == first);
}

TEST_CASE("registration job store tracks status and active jobs", "[registration][jobs]")
{
  registration::JobStore store;
  const std::string id = store.add(makeJob());

  CHECK(store.hasActiveJobs());
  CHECK(registration::isActiveJobStatus(registration::JobStatus::Queued));
  CHECK(registration::isActiveJobStatus(registration::JobStatus::ImportingOutputs));
  CHECK_FALSE(registration::isActiveJobStatus(registration::JobStatus::Completed));
  CHECK_FALSE(registration::isActiveJobStatus(registration::JobStatus::Failed));

  REQUIRE(store.setStatus(id, registration::JobStatus::Completed));
  CHECK_FALSE(store.hasActiveJobs());
}

TEST_CASE("registration job store records progress and latest message", "[registration][jobs]")
{
  registration::JobStore store;
  const std::string id = store.add(makeJob());

  REQUIRE(store.appendProgress(id, progress(0.25, "coarse level")));
  REQUIRE(store.appendProgress(id, progress(0.75, "fine level")));

  const registration::JobRecord* job = store.find(id);
  REQUIRE(job);
  REQUIRE(registration::latestProgress(*job));
  CHECK(*registration::latestProgress(*job) == 0.75);
  CHECK(registration::latestMessage(*job) == "fine level");
}

TEST_CASE("registration job store maps terminal progress events to status", "[registration][jobs]")
{
  registration::JobStore store;
  const std::string id = store.add(makeJob());

  REQUIRE(store.appendProgress(id, eventOfKind(registration::ProgressEventKind::Started)));
  REQUIRE(store.find(id));
  CHECK(store.find(id)->status == registration::JobStatus::Running);

  REQUIRE(store.appendProgress(id, eventOfKind(registration::ProgressEventKind::Completed, "done")));
  REQUIRE(store.find(id));
  CHECK(store.find(id)->status == registration::JobStatus::Completed);
}

TEST_CASE("registration job store prefers failure message over older progress text", "[registration][jobs]")
{
  registration::JobStore store;
  const std::string id = store.add(makeJob());

  REQUIRE(store.appendProgress(id, progress(0.5, "running")));
  REQUIRE(store.appendProgress(id, eventOfKind(registration::ProgressEventKind::Failed, "backend failed")));

  const registration::JobRecord* job = store.find(id);
  REQUIRE(job);
  CHECK(job->status == registration::JobStatus::Failed);
  CHECK(registration::latestMessage(*job) == "backend failed");
}

TEST_CASE("registration job store applies execution summaries", "[registration][jobs]")
{
  registration::JobStore store;
  const std::string id = store.add(makeJob());

  registration::JobExecution execution;
  execution.status = registration::JobStatus::Failed;
  execution.errorMessage = "backend failed";
  execution.warnings = {"warning"};
  execution.progressEvents = {progress(0.5)};

  REQUIRE(store.applyExecution(id, execution));

  const registration::JobRecord* job = store.find(id);
  REQUIRE(job);
  CHECK(job->status == registration::JobStatus::Failed);
  CHECK(job->errorMessage == "backend failed");
  REQUIRE(job->warnings.size() == 1);
  CHECK(job->warnings.front() == "warning");
  REQUIRE(registration::latestProgress(*job));
  CHECK(*registration::latestProgress(*job) == 0.5);
}
