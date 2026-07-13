#pragma once

#include "common/LoggingDefaults.h"

#include <spdlog/sinks/sink.h>
#include <spdlog/spdlog.h>

#include <array>
#include <span>
#include <string_view>

namespace logging
{

/**
 * @brief User-facing log verbosity choice.
 */
struct LogLevelChoice
{
  std::string_view label;             //!< Display label for UI and diagnostics
  spdlog::level::level_enum level;    //!< spdlog runtime level represented by this choice
  bool requiresCompiledTrace = false; //!< True when the choice only works if trace calls were compiled in
};

inline constexpr std::array<LogLevelChoice, 6> sk_logLevelChoices{{
  {"Critical", spdlog::level::critical, false},
  {"Error", spdlog::level::err, false},
  {"Warning", spdlog::level::warn, false},
  {"Info", spdlog::level::info, false},
  {"Debug", spdlog::level::debug, false},
  {"Trace", spdlog::level::trace, true},
}};

/**
 * @brief Return true when trace log call sites are compiled into this binary.
 * @return True for Debug builds or builds configured with Entropy_ENABLE_TRACE_LOGGING=ON.
 */
constexpr bool traceLoggingAvailable()
{
#if SPDLOG_ACTIVE_LEVEL <= SPDLOG_LEVEL_TRACE
  return true;
#else
  return false;
#endif
}

/**
 * @brief Return all log verbosity choices in most-severe to most-verbose order.
 * @return Static list containing critical through trace.
 */
constexpr std::span<const LogLevelChoice> allLogLevelChoices()
{
  return sk_logLevelChoices;
}

/**
 * @brief Return the display label for a log level.
 * @param[in] level spdlog runtime level.
 * @return User-facing label, or "Info" for unrecognized levels.
 */
constexpr std::string_view logLevelLabel(spdlog::level::level_enum level)
{
  for (const LogLevelChoice& choice : allLogLevelChoices()) {
    if (choice.level == level) {
      return choice.label;
    }
  }

  return "Info";
}

/**
 * @brief Return true when a log level can be selected in this build.
 * @param[in] choice Candidate log level choice.
 * @return False for trace in builds where trace calls are compiled out.
 */
constexpr bool isLogLevelChoiceAvailable(const LogLevelChoice& choice)
{
  return !choice.requiresCompiledTrace || traceLoggingAvailable();
}

/**
 * @brief Return the selectable replacement for an unavailable log level.
 * @param[in] level Requested or current spdlog runtime level.
 * @return @p level when available, otherwise debug for unavailable trace.
 */
constexpr spdlog::level::level_enum selectableLogLevel(spdlog::level::level_enum level)
{
  if (spdlog::level::trace == level && !traceLoggingAvailable()) {
    return spdlog::level::debug;
  }

  return level;
}

/**
 * @brief Apply a runtime log level to every sink owned by the default logger.
 * @param[in] level Desired runtime log level.
 */
inline void setDefaultLoggerSinkLevel(spdlog::level::level_enum level)
{
  if (auto logger = spdlog::default_logger()) {
    for (const auto& sink : logger->sinks()) {
      if (sink) {
        sink->set_level(level);
      }
    }
  }
}

/**
 * @brief Return the current runtime sink log level for the default logger.
 * @return First sink level when available; otherwise the build default log level.
 */
inline spdlog::level::level_enum defaultLoggerSinkLevel()
{
  if (auto logger = spdlog::default_logger(); logger && !logger->sinks().empty() && logger->sinks().front()) {
    return selectableLogLevel(logger->sinks().front()->level());
  }

  return defaultLogLevel();
}

} // namespace logging
