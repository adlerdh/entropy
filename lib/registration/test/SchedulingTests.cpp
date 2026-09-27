#include "registration/Scheduling.h"
#include <catch2/catch_test_macros.hpp>
#include <vector>
#include <string>

TEST_CASE("Saturated registration scheduling still propagates cancellation", "[workflow][registration][threading]")
{
  std::vector<registration::JobRecord> jobs(2);
  jobs[0].id = "running";
  jobs[0].status = registration::JobStatus::Cancelled;
  jobs[1].id = "queued";
  std::vector<std::string> cancellations;
  unsigned launches = 0;
  registration::dispatchQueuedJobs(
    jobs,
    1,
    1,
    [](const auto&) { return true; },
    [&](const auto& id) { cancellations.push_back(id); },
    [&](const auto&) {
      ++launches;
      return true;
    });
  CHECK(cancellations == std::vector<std::string>{"running"});
  CHECK(launches == 0);
}

TEST_CASE(
  "Registration dispatch skips duplicate running jobs and reuses failed preparation slots",
  "[workflow][registration]")
{
  std::vector<registration::JobRecord> jobs(4);
  jobs[0].id = "duplicate";
  jobs[1].id = "broken-input";
  jobs[2].id = "ready";
  jobs[3].id = "waiting";
  std::vector<std::string> prepared;
  registration::dispatchQueuedJobs(
    jobs,
    1,
    2,
    [](const auto& id) { return id == "duplicate"; },
    [](const auto&) { FAIL("Unexpected cancellation"); },
    [&](const auto& job) {
      prepared.push_back(job.id);
      return job.id != "broken-input";
    });
  CHECK(prepared == std::vector<std::string>{"broken-input", "ready"});
}
