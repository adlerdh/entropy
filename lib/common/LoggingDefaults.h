#pragma once

#include <spdlog/spdlog.h>

#ifndef ENTROPY_DEFAULT_LOG_LEVEL
#define ENTROPY_DEFAULT_LOG_LEVEL SPDLOG_LEVEL_INFO
#endif

namespace logging
{

/**
 * @brief Return the default runtime log level for this build configuration.
 *
 * Debug builds default to debug logging, while release-like builds default to info logging.
 *
 * @return Default spdlog runtime level selected at compile time.
 */
constexpr spdlog::level::level_enum defaultLogLevel()
{
#if ENTROPY_DEFAULT_LOG_LEVEL == SPDLOG_LEVEL_DEBUG
  return spdlog::level::debug;
#else
  return spdlog::level::info;
#endif
}

/**
 * @brief Return the command-line spelling of the default runtime log level.
 * @return "debug" for Debug builds and "info" for release-like builds.
 */
constexpr const char* defaultLogLevelName()
{
#if ENTROPY_DEFAULT_LOG_LEVEL == SPDLOG_LEVEL_DEBUG
  return "debug";
#else
  return "info";
#endif
}

} // namespace logging
