#include "EntropyApp.h"
#include "common/InputParser.h"
#include "common/LoggingSettings.h"
#include "logic/app/AppPaths.h"
#include "logic/app/Logging.h"
#include "logic/app/StackTrace.h"

#include <spdlog/fmt/ostr.h>
#include <spdlog/spdlog.h>

#include <cstdlib>
#include <exception>
#include <iostream>

int main(int argc, char* argv[])
{
  stack_trace::installCrashHandlers();
  app_paths::configureFromCommandLine(argc, argv);

  auto logFailure = []() {
    spdlog::debug("------------------- End session (failure) -------------------");
  };

  try {
    Logging{}.setup();
  }
  catch (const std::exception& e) {
    std::cerr << "[critical] Exception while setting up application logging: " << e.what()
              << ". Entropy cannot start\n";
    return EXIT_FAILURE;
  }
  catch (...) {
    std::cerr << "[critical] Unknown exception while setting up application logging. Entropy cannot start\n";
    return EXIT_FAILURE;
  }

  try {
    spdlog::debug("------------------- Begin session -------------------");
    EntropyApp::logPreamble();

    InputParams params;

    bool exitRequested = false;
    if (!parseCommandLine(argc, argv, params, &exitRequested)) {
      logFailure();
      return EXIT_FAILURE;
    }
    if (exitRequested) {
      return EXIT_SUCCESS;
    }

    logging::setApplicationLogLevel(params.logLevel);

    spdlog::debug("Parsed command line parameters:\n{}", params);

    EntropyApp app;
    app.init();

    if (params.set) {
      app.loadImagesFromParams(params);
    }
    app.run();
    spdlog::info("Entropy is exiting normally");
  }
  catch (const std::runtime_error& e) {
    spdlog::critical("Unhandled runtime error; Entropy is terminating: {}", e.what());
    logFailure();
    return EXIT_FAILURE;
  }
  catch (const std::exception& e) {
    spdlog::critical("Unhandled exception; Entropy is terminating: {}", e.what());
    logFailure();
    return EXIT_FAILURE;
  }
  catch (...) {
    spdlog::critical("Unknown unhandled exception; Entropy is terminating");
    logFailure();
    return EXIT_FAILURE;
  }

  spdlog::debug("------------------- End session (success) -------------------");
  return EXIT_SUCCESS;
}
