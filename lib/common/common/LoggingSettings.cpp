#include "common/LoggingSettings.h"

#include <spdlog/sinks/sink.h>
#include <spdlog/spdlog.h>

#include <atomic>

namespace logging
{
namespace
{
std::atomic<spdlog::level::level_enum> g_configuredLogLevel{defaultLogLevel()};
std::atomic_bool g_loggingEnabled{true};

void applyDefaultLoggerSinkConfiguration()
{
  const spdlog::level::level_enum level = g_loggingEnabled.load(std::memory_order_relaxed)
                                            ? g_configuredLogLevel.load(std::memory_order_relaxed)
                                            : spdlog::level::off;
  if (auto logger = spdlog::default_logger()) {
    for (const auto& sink : logger->sinks()) {
      if (sink) {
        sink->set_level(level);
      }
    }
  }
}
} // namespace

bool loggingEnabled()
{
  return g_loggingEnabled.load(std::memory_order_relaxed);
}

void setLoggingEnabled(bool enabled)
{
  const bool previous = g_loggingEnabled.exchange(enabled, std::memory_order_relaxed);
  if (previous == enabled) {
    applyDefaultLoggerSinkConfiguration();
    return;
  }

  if (!enabled) {
    spdlog::info("Application logging disabled; no further Entropy log messages will be written until it is enabled");
    if (auto logger = spdlog::default_logger()) {
      logger->flush();
    }
  }

  applyDefaultLoggerSinkConfiguration();
  if (enabled) {
    spdlog::info("Application logging enabled at {} verbosity", logLevelLabel(g_configuredLogLevel.load()));
  }
}

void setApplicationLogLevel(spdlog::level::level_enum level)
{
  if (level == spdlog::level::off) {
    setLoggingEnabled(false);
    return;
  }

  g_configuredLogLevel.store(selectableLogLevel(level), std::memory_order_relaxed);
  applyDefaultLoggerSinkConfiguration();
}

spdlog::level::level_enum applicationLogLevel()
{
  return g_configuredLogLevel.load(std::memory_order_relaxed);
}

} // namespace logging
