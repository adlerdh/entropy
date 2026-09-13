#include "rendering/FramePacer.h"

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <vector>

namespace
{
using FramePacer = rendering::FramePacer;
using Duration = FramePacer::Duration;
using TimePoint = FramePacer::TimePoint;

TEST_CASE("Frame pacing does nothing when disabled")
{
  int clockReads = 0;
  int sleeps = 0;
  FramePacer pacer(
    [&] {
      ++clockReads;
      return TimePoint{};
    },
    [&](Duration) { ++sleeps; });

  TimePoint lastFrame{std::chrono::seconds{4}};
  const TimePoint original = lastFrame;
  pacer.wait({.enabled = false, .targetFrameTime = Duration{1.0 / 60.0}}, lastFrame);

  CHECK(lastFrame == original);
  CHECK(clockReads == 0);
  CHECK(sleeps == 0);
}

TEST_CASE("Frame pacing sleeps only for the remainder of the frame interval")
{
  TimePoint current{std::chrono::seconds{10}};
  std::vector<Duration> sleeps;
  FramePacer pacer(
    [&] { return current; },
    [&](Duration duration) {
      sleeps.push_back(duration);
      current += std::chrono::duration_cast<FramePacer::Clock::duration>(duration);
    });

  TimePoint lastFrame = current - std::chrono::milliseconds{6};
  pacer.wait({.enabled = true, .targetFrameTime = std::chrono::milliseconds{10}}, lastFrame);

  REQUIRE(sleeps.size() == 1);
  CHECK(sleeps.front() == Duration{0.004});
  CHECK(lastFrame == current);
}

TEST_CASE("Frame pacing does not sleep after the target interval has elapsed")
{
  const TimePoint current{std::chrono::seconds{10}};
  int sleeps = 0;
  FramePacer pacer([&] { return current; }, [&](Duration) { ++sleeps; });

  TimePoint lastFrame = current - std::chrono::milliseconds{20};
  pacer.wait({.enabled = true, .targetFrameTime = std::chrono::milliseconds{10}}, lastFrame);

  CHECK(sleeps == 0);
  CHECK(lastFrame == current);
}

TEST_CASE("Nonpositive target frame times never request a sleep")
{
  const TimePoint current{std::chrono::seconds{10}};
  int sleeps = 0;
  FramePacer pacer([&] { return current; }, [&](Duration) { ++sleeps; });

  TimePoint lastFrame{};
  pacer.wait({.enabled = true, .targetFrameTime = Duration::zero()}, lastFrame);

  CHECK(sleeps == 0);
  CHECK(lastFrame == current);
}
} // namespace
