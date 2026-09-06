#pragma once

#include <chrono>
#include <functional>

namespace rendering
{

/** Settings used to pace presentation of application frames. */
struct FramePacingSettings
{
  bool enabled = false;
  std::chrono::duration<double> targetFrameTime{1.0 / 60.0};
};

/**
 * @brief Monotonic, host-independent frame pacing service.
 *
 * Time acquisition and sleeping are injected so pacing policy can be tested without
 * sleeping a test process. The default constructor uses the steady system clock.
 */
class FramePacer
{
public:
  using Clock = std::chrono::steady_clock;
  using TimePoint = Clock::time_point;
  using Duration = std::chrono::duration<double>;
  using NowFunction = std::function<TimePoint()>;
  using SleepFunction = std::function<void(Duration)>;

  FramePacer();
  FramePacer(NowFunction now, SleepFunction sleep);

  /**
   * Wait for the remainder of the configured frame interval and record presentation time.
   * Disabled pacing leaves `lastFrameTime` unchanged.
   */
  void wait(const FramePacingSettings& settings, TimePoint& lastFrameTime) const;

private:
  NowFunction m_now;
  SleepFunction m_sleep;
};

} // namespace rendering
