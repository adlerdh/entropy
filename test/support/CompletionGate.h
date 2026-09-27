#pragma once

#include <chrono>
#include <future>
#include <functional>
#include <memory>

namespace entropy::test
{
/// Coordinate a worker checkpoint with bounded waits on the test thread.
/// Declare after the worker owner so assertion unwinding releases the worker before the owner's joining destructor.
/// Invoke the waiter once per gate; call awaitEntry() and release() from the controlling test thread.
class CompletionGate
{
public:
  /// Create an unreleased gate whose worker has not yet entered.
  CompletionGate() : m_state(std::make_shared<State>()), m_enteredFuture(m_state->entered.get_future()) {}

  /// Release the worker during normal destruction or assertion unwinding.
  ~CompletionGate()
  {
    release();
  }

  /// Return a worker callback that signals entry and blocks until release().
  /// Shared state keeps the callback valid after the gate is destroyed.
  std::function<void()> waiter() const
  {
    return [state = m_state] {
      state->entered.set_value();
      state->releaseFuture.wait();
    };
  }

  /// Wait at most timeout for the worker checkpoint; return false if the deadline expires.
  bool awaitEntry(std::chrono::milliseconds timeout = std::chrono::seconds{5})
  {
    return m_enteredFuture.wait_for(timeout) == std::future_status::ready;
  }

  /// Unblock the worker; repeated calls on the controlling thread are harmless.
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
