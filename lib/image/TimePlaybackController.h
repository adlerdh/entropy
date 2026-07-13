#pragma once

#include <cstdint>

/**
 * @brief State retained between time-series playback ticks.
 */
struct TimePlaybackState
{
  double lastAdvanceTimeSeconds = 0.0; //!< Wall-clock time of the previous frame advance
};

/**
 * @brief Inputs needed to advance a time-series playback frame.
 */
struct TimePlaybackInput
{
  bool playing = false;            //!< Whether playback is currently running
  bool loop = true;                //!< Whether playback wraps from the last frame to the first
  uint32_t activeTimePoint = 0;    //!< Currently displayed time point
  uint32_t numTimePoints = 1;      //!< Number of available time points
  double framePeriodSeconds = 0.1; //!< Playback period for one frame
  double nowSeconds = 0.0;         //!< Current monotonic time in seconds
};

/**
 * @brief Result of one time-series playback update.
 */
struct TimePlaybackUpdate
{
  uint32_t timePoint = 0;      //!< Time point that should be displayed after the update
  bool playing = false;        //!< Whether playback should remain active
  bool advanced = false;       //!< True when the displayed frame changed
  bool playingChanged = false; //!< True when playback should be started or stopped
};

/**
 * @brief Advance to the next time point, respecting the loop setting.
 * @param activeTimePoint Currently displayed time point.
 * @param numTimePoints Number of available time points.
 * @param loop Whether to wrap at the end.
 * @return Next time point.
 */
uint32_t nextPlaybackTimePoint(uint32_t activeTimePoint, uint32_t numTimePoints, bool loop);

/**
 * @brief Advance time-series playback if enough wall-clock time has elapsed.
 * @param state Mutable playback state retained between calls.
 * @param input Playback inputs for this tick.
 * @return Requested playback update.
 */
TimePlaybackUpdate updateTimePlaybackFrame(TimePlaybackState& state, const TimePlaybackInput& input);

/**
 * @brief Compute the effective playback rate.
 * @param framePeriodSeconds Playback period for one frame.
 * @return Playback rate in frames per second, or 0.0 when the period is invalid.
 */
double playbackFramesPerSecond(double framePeriodSeconds);
