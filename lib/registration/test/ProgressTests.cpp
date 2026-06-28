#include "registration/Progress.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("registration progress events parse from JSON lines", "[registration][progress]")
{
  registration::ProgressEvent event;
  event.kind = registration::ProgressEventKind::Progress;
  event.jobId = "job";
  event.stageName = "deformable";
  event.iteration = 4;
  event.iterations = 10;
  event.progress = 0.4;

  const std::string line = registration::progressEventLine(event);
  std::string error;
  const std::optional<registration::ProgressEvent> restored = registration::parseProgressEventLine(line, &error);

  REQUIRE(restored);
  CHECK(error.empty());
  CHECK(restored->kind == registration::ProgressEventKind::Progress);
  CHECK(restored->jobId == "job");
  REQUIRE(restored->progress);
  CHECK(*restored->progress == 0.4);
}

TEST_CASE("registration progress parser ignores blank lines", "[registration][progress]")
{
  std::string error;
  const std::optional<registration::ProgressEvent> event = registration::parseProgressEventLine("   \t", &error);

  CHECK_FALSE(event);
  CHECK(error.empty());
}

TEST_CASE("registration progress parser reports malformed lines", "[registration][progress]")
{
  std::string error;
  const std::optional<registration::ProgressEvent> event = registration::parseProgressEventLine("not json", &error);

  CHECK_FALSE(event);
  CHECK_FALSE(error.empty());
}
