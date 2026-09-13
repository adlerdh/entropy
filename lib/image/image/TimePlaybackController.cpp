#include "image/TimePlaybackController.h"

#include <algorithm>
#include <cmath>

namespace
{
double safeFramePeriod(double framePeriodSeconds)
{
  return std::isfinite(framePeriodSeconds) && framePeriodSeconds > 0.0 ? framePeriodSeconds : 0.1;
}
} // namespace

uint32_t nextPlaybackTimePoint(uint32_t activeTimePoint, uint32_t numTimePoints, bool loop)
{
  if (numTimePoints <= 1u) {
    return 0u;
  }

  const uint32_t maxTimePoint = numTimePoints - 1u;
  const uint32_t clamped = std::min(activeTimePoint, maxTimePoint);
  if (clamped < maxTimePoint) {
    return clamped + 1u;
  }
  return loop ? 0u : maxTimePoint;
}

TimePlaybackUpdate updateTimePlaybackFrame(TimePlaybackState& state, const TimePlaybackInput& input)
{
  const uint32_t numTimePoints = std::max(1u, input.numTimePoints);
  const uint32_t maxTimePoint = numTimePoints - 1u;
  const uint32_t activeTimePoint = std::min(input.activeTimePoint, maxTimePoint);
  TimePlaybackUpdate update{
    .timePoint = activeTimePoint,
    .playing = input.playing,
    .advanced = false,
    .playingChanged = false};

  if (!input.playing || numTimePoints <= 1u) {
    state.lastAdvanceTimeSeconds = 0.0;
    return update;
  }

  if (state.lastAdvanceTimeSeconds <= 0.0) {
    state.lastAdvanceTimeSeconds = input.nowSeconds;
    return update;
  }

  if (input.nowSeconds - state.lastAdvanceTimeSeconds < safeFramePeriod(input.framePeriodSeconds)) {
    return update;
  }

  const uint32_t requestedTimePoint = nextPlaybackTimePoint(activeTimePoint, numTimePoints, input.loop);
  if (requestedTimePoint == activeTimePoint && !input.loop) {
    state.lastAdvanceTimeSeconds = 0.0;
    update.playing = false;
    update.playingChanged = true;
    return update;
  }

  state.lastAdvanceTimeSeconds = input.nowSeconds;
  update.timePoint = requestedTimePoint;
  update.advanced = requestedTimePoint != activeTimePoint;
  return update;
}

double playbackFramesPerSecond(double framePeriodSeconds)
{
  return 1.0 / safeFramePeriod(framePeriodSeconds);
}
