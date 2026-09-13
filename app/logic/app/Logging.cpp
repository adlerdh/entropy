#include "logic/app/Logging.h"
#include "common/Exception.hpp"
#include "common/LoggingSettings.h"
#include "logic/app/AppPaths.h"

#include <spdlog/fmt/std.h>
#include <spdlog/sinks/daily_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include <filesystem>
#include <memory>
#include <sstream>

void Logging::setup()
{
  try {
    const std::filesystem::path logDir = app_paths::logDirectory();
    std::filesystem::create_directories(logDir);
    const std::filesystem::path logFileName = logDir / "entropy.txt";

    // Create multi-threaded sinks for console and daily file logging. Start them disabled so the
    // complete configuration is applied atomically after the new default logger is installed.

    auto consoleSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    consoleSink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [tid %t] [%^%l%$] %v");
    consoleSink->set_level(spdlog::level::off);

    // The daily file sink includes the logger name and time zone. Source file and line are included
    // when the call site uses an SPDLOG_* macro that supplies source-location metadata.
    auto dailySink = std::make_shared<spdlog::sinks::daily_file_sink_mt>(logFileName.string(), 23, 59);
    dailySink->set_pattern("[%Y-%m-%d %H:%M:%S.%e %z] [%n] [tid %t] [%l] [%s:%#] %v");
    dailySink->set_level(spdlog::level::off);

    spdlog::sinks_init_list sinkList{consoleSink, dailySink};

    // Create synchronous loggers sharing the same sinks
    auto defaultLogger = std::make_shared<spdlog::logger>("default", std::begin(sinkList), std::end(sinkList));

    defaultLogger->set_level(spdlog::level::trace);
    defaultLogger->flush_on(spdlog::level::debug);

    // Register for global access with spdlog::get
    spdlog::register_logger(defaultLogger);
    spdlog::set_default_logger(defaultLogger);
    logging::setApplicationLogLevel(logging::defaultLogLevel());
  }
  catch (const spdlog::spdlog_ex& e) {
    std::ostringstream ss;
    ss << "Logging construction failed: " << e.what() << std::ends;
    throwDebug(ss.str());
  }
  catch (const std::exception& e) {
    std::ostringstream ss;
    ss << "Logging construction failed: " << e.what() << std::ends;
    throwDebug(ss.str());
  }

  spdlog::debug("Initialized console and daily file logging in {}", app_paths::logDirectory());
}
