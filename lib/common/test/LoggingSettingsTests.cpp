#include "common/LoggingSettings.h"

#include <catch2/catch_test_macros.hpp>

#include <spdlog/sinks/null_sink.h>

#include <algorithm>
#include <memory>

TEST_CASE("log level choices are ordered from critical through trace", "[common][logging]")
{
  const auto choices = entropy::logging::allLogLevelChoices();

  REQUIRE(choices.size() == 6);
  CHECK(choices[0].level == spdlog::level::critical);
  CHECK(choices[1].level == spdlog::level::err);
  CHECK(choices[2].level == spdlog::level::warn);
  CHECK(choices[3].level == spdlog::level::info);
  CHECK(choices[4].level == spdlog::level::debug);
  CHECK(choices[5].level == spdlog::level::trace);
}

TEST_CASE("trace log level availability matches compile-time trace support", "[common][logging]")
{
  const auto choices = entropy::logging::allLogLevelChoices();
  const auto traceChoice = std::find_if(choices.begin(), choices.end(), [](const auto& choice) {
    return choice.level == spdlog::level::trace;
  });

  REQUIRE(traceChoice != choices.end());
  CHECK(entropy::logging::isLogLevelChoiceAvailable(*traceChoice) == entropy::logging::traceLoggingAvailable());
}

TEST_CASE("unavailable trace level is represented as debug in selectable UI state", "[common][logging]")
{
  const auto selectableTrace = entropy::logging::selectableLogLevel(spdlog::level::trace);

  if (entropy::logging::traceLoggingAvailable()) {
    CHECK(selectableTrace == spdlog::level::trace);
  }
  else {
    CHECK(selectableTrace == spdlog::level::debug);
  }
}

TEST_CASE("log level labels cover known levels and default unknown levels to info", "[common][logging]")
{
  CHECK(entropy::logging::logLevelLabel(spdlog::level::critical) == "Critical");
  CHECK(entropy::logging::logLevelLabel(spdlog::level::err) == "Error");
  CHECK(entropy::logging::logLevelLabel(spdlog::level::warn) == "Warning");
  CHECK(entropy::logging::logLevelLabel(spdlog::level::info) == "Info");
  CHECK(entropy::logging::logLevelLabel(spdlog::level::debug) == "Debug");
  CHECK(entropy::logging::logLevelLabel(spdlog::level::trace) == "Trace");
  CHECK(entropy::logging::logLevelLabel(spdlog::level::off) == "Info");
}

TEST_CASE("default logger sink level helpers read and update sink levels", "[common][logging]")
{
  const auto previousLogger = spdlog::default_logger();
  const auto sink = std::make_shared<spdlog::sinks::null_sink_mt>();
  const auto logger = std::make_shared<spdlog::logger>("common-test-logger", sink);
  spdlog::set_default_logger(logger);

  entropy::logging::setDefaultLoggerSinkLevel(spdlog::level::warn);
  CHECK(entropy::logging::defaultLoggerSinkLevel() == spdlog::level::warn);

  entropy::logging::setDefaultLoggerSinkLevel(spdlog::level::trace);
  CHECK(entropy::logging::defaultLoggerSinkLevel() == entropy::logging::selectableLogLevel(spdlog::level::trace));

  spdlog::set_default_logger(previousLogger);
}
