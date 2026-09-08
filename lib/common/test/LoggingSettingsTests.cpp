#include "common/LoggingSettings.h"

#include <catch2/catch_test_macros.hpp>

#include <spdlog/sinks/null_sink.h>

#include <algorithm>
#include <memory>

TEST_CASE("log level choices are ordered from critical through trace", "[common][logging]")
{
  const auto choices = logging::allLogLevelChoices();

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
  const auto choices = logging::allLogLevelChoices();
  const auto traceChoice = std::find_if(choices.begin(), choices.end(), [](const auto& choice) {
    return choice.level == spdlog::level::trace;
  });

  REQUIRE(traceChoice != choices.end());
  CHECK(logging::isLogLevelChoiceAvailable(*traceChoice) == logging::traceLoggingAvailable());
}

TEST_CASE("available log level choices omit trace when trace calls are compiled out", "[common][logging]")
{
  const auto choices = logging::availableLogLevelChoices();
  REQUIRE_FALSE(choices.empty());
  CHECK((choices.back().level == spdlog::level::trace) == logging::traceLoggingAvailable());
}

TEST_CASE("unavailable trace level is represented as debug in selectable UI state", "[common][logging]")
{
  const auto selectableTrace = logging::selectableLogLevel(spdlog::level::trace);

  if (logging::traceLoggingAvailable()) {
    CHECK(selectableTrace == spdlog::level::trace);
  }
  else {
    CHECK(selectableTrace == spdlog::level::debug);
  }
}

TEST_CASE("log level labels cover known levels and default unknown levels to info", "[common][logging]")
{
  CHECK(logging::logLevelLabel(spdlog::level::critical) == "Critical");
  CHECK(logging::logLevelLabel(spdlog::level::err) == "Error");
  CHECK(logging::logLevelLabel(spdlog::level::warn) == "Warning");
  CHECK(logging::logLevelLabel(spdlog::level::info) == "Info");
  CHECK(logging::logLevelLabel(spdlog::level::debug) == "Debug");
  CHECK(logging::logLevelLabel(spdlog::level::trace) == "Trace");
  CHECK(logging::logLevelLabel(spdlog::level::off) == "Info");
}

TEST_CASE("logging settings preserve verbosity while globally disabled", "[common][logging]")
{
  const auto previousLogger = spdlog::default_logger();
  const bool previousEnabled = logging::loggingEnabled();
  const auto previousLevel = logging::applicationLogLevel();
  const auto firstSink = std::make_shared<spdlog::sinks::null_sink_mt>();
  const auto secondSink = std::make_shared<spdlog::sinks::null_sink_mt>();
  const auto logger =
    std::make_shared<spdlog::logger>("common-test-logger", spdlog::sinks_init_list{firstSink, secondSink});
  spdlog::set_default_logger(logger);

  logging::setLoggingEnabled(true);
  logging::setApplicationLogLevel(spdlog::level::warn);
  CHECK(logging::applicationLogLevel() == spdlog::level::warn);
  CHECK(firstSink->level() == spdlog::level::warn);
  CHECK(secondSink->level() == spdlog::level::warn);

  logging::setApplicationLogLevel(spdlog::level::off);
  CHECK_FALSE(logging::loggingEnabled());
  CHECK(logging::applicationLogLevel() == spdlog::level::warn);
  CHECK(firstSink->level() == spdlog::level::off);
  CHECK(secondSink->level() == spdlog::level::off);

  logging::setApplicationLogLevel(spdlog::level::debug);
  CHECK(logging::applicationLogLevel() == spdlog::level::debug);
  CHECK(firstSink->level() == spdlog::level::off);
  CHECK(secondSink->level() == spdlog::level::off);

  logging::setLoggingEnabled(true);
  CHECK(firstSink->level() == spdlog::level::debug);
  CHECK(secondSink->level() == spdlog::level::debug);

  logging::setApplicationLogLevel(previousLevel);
  logging::setLoggingEnabled(previousEnabled);
  spdlog::set_default_logger(previousLogger);
}
