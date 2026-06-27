#include "image/TimePlaybackController.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

TEST_CASE("Time playback advances only after one frame period", "[image][time]")
{
  TimePlaybackState state;
  TimePlaybackInput input{
    .playing = true,
    .loop = true,
    .activeTimePoint = 0,
    .numTimePoints = 4,
    .framePeriodSeconds = 0.5,
    .nowSeconds = 10.0};

  auto update = updateTimePlaybackFrame(state, input);
  CHECK(update.timePoint == 0);
  CHECK(update.playing);
  CHECK_FALSE(update.advanced);

  input.nowSeconds = 10.4;
  update = updateTimePlaybackFrame(state, input);
  CHECK(update.timePoint == 0);
  CHECK_FALSE(update.advanced);

  input.nowSeconds = 10.5;
  update = updateTimePlaybackFrame(state, input);
  CHECK(update.timePoint == 1);
  CHECK(update.advanced);
}

TEST_CASE("Time playback loops or stops at the last frame", "[image][time]")
{
  TimePlaybackState state;
  TimePlaybackInput input{
    .playing = true,
    .loop = true,
    .activeTimePoint = 2,
    .numTimePoints = 3,
    .framePeriodSeconds = 0.1,
    .nowSeconds = 1.0};

  (void)updateTimePlaybackFrame(state, input);
  input.nowSeconds = 1.1;
  auto update = updateTimePlaybackFrame(state, input);
  CHECK(update.timePoint == 0);
  CHECK(update.playing);
  CHECK(update.advanced);

  state = {};
  input.loop = false;
  input.nowSeconds = 2.0;
  (void)updateTimePlaybackFrame(state, input);
  input.nowSeconds = 2.1;
  update = updateTimePlaybackFrame(state, input);
  CHECK(update.timePoint == 2);
  CHECK_FALSE(update.playing);
  CHECK(update.playingChanged);
  CHECK_FALSE(update.advanced);
}

TEST_CASE("Playback frame rate is derived from frame period", "[image][time]")
{
  CHECK(playbackFramesPerSecond(0.5) == Catch::Approx(2.0));
  CHECK(playbackFramesPerSecond(0.0) == Catch::Approx(10.0));
  CHECK(nextPlaybackTimePoint(3, 4, true) == 0);
  CHECK(nextPlaybackTimePoint(3, 4, false) == 3);
  CHECK(nextPlaybackTimePoint(99, 4, true) == 0);
}
