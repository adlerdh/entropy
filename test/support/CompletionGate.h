#pragma once

#include <chrono>
#include <future>
#include <functional>
#include <memory>

namespace entropy::test
{
// Declare after the worker owner so assertion unwinding always releases its worker
// before the owner's joining destructor. No sleeps or unbounded test-thread waits.
class CompletionGate
{
public:
  CompletionGate() : m_state(std::make_shared<State>()), m_enteredFuture(m_state->entered.get_future()) {}

  ~CompletionGate()
  {
    release();
  }

  std::function<void()> waiter() const
  {
    return [state = m_state] {
      state->entered.set_value();
      state->releaseFuture.wait();
    };
  }

  bool awaitEntry(std::chrono::milliseconds timeout = std::chrono::seconds{5})
  {
    return m_enteredFuture.wait_for(timeout) == std::future_status::ready;
  }

  void release()
  {
    if (!m_released) {
      m_state->release.set_value();
      m_released = true;
    }
  }

private:
  struct State
  {
    std::promise<void> entered;
    std::promise<void> release;
    std::shared_future<void> releaseFuture{release.get_future().share()};
  };
  
  std::shared_ptr<State> m_state;
  std::future<void> m_enteredFuture;
  bool m_released = false;
};
} // namespace entropy::test
