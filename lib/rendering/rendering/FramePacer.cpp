#include "rendering/FramePacer.h"

#include <compare>
#include <thread>
#include <utility>

namespace rendering
{

FramePacer::FramePacer()
  : FramePacer([] { return Clock::now(); }, [](Duration duration) { std::this_thread::sleep_for(duration); })
{
}

FramePacer::FramePacer(NowFunction now, SleepFunction sleepFunction)
  : m_now(std::move(now)), m_sleep(std::move(sleepFunction))
{
}

void FramePacer::wait(const FramePacingSettings& settings, TimePoint& lastFrameTime) const
{
  if (!settings.enabled) {
    return;
  }

  const TimePoint now = m_now();
  const Duration elapsed = now - lastFrameTime;
  if (settings.targetFrameTime > Duration::zero() && elapsed < settings.targetFrameTime) {
    m_sleep(settings.targetFrameTime - elapsed);
  }

  lastFrameTime = m_now();
}

} // namespace rendering
