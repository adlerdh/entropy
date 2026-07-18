#include "EntropyApp.h"
#include "common/InputParser.h"
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

  Logging logging;

  try {
    logging.setup();
  }
  catch (const std::exception& e) {
    std::cerr << "Exception when setting up logger: " << e.what() << '\n';
    return EXIT_FAILURE;
  }
  catch (...) {
    std::cerr << "Unknown exception when setting up logger" << '\n';
    return EXIT_FAILURE;
  }

  try {
    spdlog::debug("------------------- Begin session -------------------");
    EntropyApp::logPreamble();

    InputParams params;

    if (!parseCommandLine(argc, argv, params)) {
      logFailure();
      return EXIT_FAILURE;
    }

    logging.setConsoleSinkLevel(params.consoleLogLevel);
    logging.setDailyFileSinkLevel(params.consoleLogLevel);

    spdlog::debug("Parsed command line parameters:\n{}", params);

    EntropyApp app;
    app.init();

    if (params.set) {
      app.loadImagesFromParams(params);
    }
    app.run();
  }
  catch (const std::runtime_error& e) {
    spdlog::critical("Runtime error: {}", e.what());
    logFailure();
    return EXIT_FAILURE;
  }
  catch (const std::exception& e) {
    spdlog::critical("Exception: {}", e.what());
    logFailure();
    return EXIT_FAILURE;
  }
  catch (...) {
    spdlog::critical("Unknown exception");
    logFailure();
    return EXIT_FAILURE;
  }

  spdlog::debug("------------------- End session (success) -------------------");
  return EXIT_SUCCESS;
}
