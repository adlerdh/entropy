#include "logic/app/Logging.h"
#include "common/Exception.hpp"

#include <sstream>

void Logging::setup()
{
  static const std::string sk_logFileName("logs/entropy.txt");

  try
  {
    // Create multi-threaded sinks for console and daily file logging.
    // Assign default sink logging levels.

    m_console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    m_console_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [tid %t] [%^%l%$] %v");
    m_console_sink->set_level(spdlog::level::info); // default to info level

    // The daily file sink uses shows more info: logger name and time zone.
    // Note: debug logging needs SPDLOG_XXX macro
    m_daily_sink = std::make_shared<spdlog::sinks::daily_file_sink_mt>(sk_logFileName, 23, 59);
    m_daily_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e %z] [%n] [tid %t] [%l] [%s:%#] %v");
    m_daily_sink->set_level(spdlog::level::debug); // default to debug level

    spdlog::sinks_init_list sink_list{m_console_sink, m_daily_sink};

    // Create synchronous loggers sharing the same sinks
    auto default_logger
      = std::make_shared<spdlog::logger>("default", std::begin(sink_list), std::end(sink_list));

    default_logger->set_level(spdlog::level::trace);
    default_logger->flush_on(spdlog::level::debug);

    // Register for global access with spdlog::get
    spdlog::register_logger(default_logger);
    spdlog::set_default_logger(default_logger);
  }
  catch (const spdlog::spdlog_ex& e)
  {
    std::ostringstream ss;
    ss << "Logging construction failed: " << e.what() << std::ends;
    throw_debug(ss.str())
  }
  catch (const std::exception& e)
  {
    std::ostringstream ss;
    ss << "Logging construction failed: " << e.what() << std::ends;
    throw_debug(ss.str())
  }

  spdlog::debug("Setup the logger");
}

void Logging::setConsoleSinkLevel(spdlog::level::level_enum level)
{
  if (m_console_sink)
  {
    m_console_sink->set_level(level);
  }
  else
  {
    spdlog::error("Console logging sink is null");
  }
}

void Logging::setDailyFileSinkLevel(spdlog::level::level_enum level)
{
  if (m_daily_sink)
  {
    m_daily_sink->set_level(level);
  }
  else
  {
    spdlog::error("Daily file logging sink is null");
  }
}
