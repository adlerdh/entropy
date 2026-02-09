#include "EntropyApp.h"
#include "common/InputParser.h"
#include "logic/app/Logging.h"

#include <spdlog/fmt/ostr.h>
#include <spdlog/spdlog.h>

int main(int argc, char* argv[])
{
  auto logFailure = []() {
    spdlog::debug("------------------- End session (failure) -------------------");
  };

  Logging logging;

  try {
    logging.setup();
  }
  catch (const std::exception& e) {
    std::cerr << "Exception when setting up logger: " << e.what() << std::endl;
    return EXIT_FAILURE;
  }
  catch (...) {
    std::cerr << "Unknown exception when setting up logger" << std::endl;
    return EXIT_FAILURE;
  }

  try
  {
    spdlog::debug("------------------- Being session -------------------");
    EntropyApp::logPreamble();

    InputParams params;

    if (!parseCommandLine(argc, argv, params)) {
      logFailure();
      return EXIT_FAILURE;
    }

    if (!params.set) {
      spdlog::debug("Command line arguments not specified");
      logFailure();
      return EXIT_FAILURE;
    }

    logging.setConsoleSinkLevel(params.consoleLogLevel);
    logging.setDailyFileSinkLevel(params.consoleLogLevel);

    spdlog::debug("Parsed command line parameters:\n{}", params);

    EntropyApp app;
    app.loadImagesFromParams(params);
    app.init();
    app.run();
  }
  catch (const std::runtime_error& e)
  {
    spdlog::critical("Runtime error: {}", e.what());
    /// @todo use https://en.cppreference.com/w/cpp/utility/basic_stacktrace
    logFailure();
    return EXIT_FAILURE;
  }
  catch (const std::exception& e)
  {
    spdlog::critical("Exception: {}", e.what());
    /// @todo use https://en.cppreference.com/w/cpp/utility/basic_stacktrace
    logFailure();
    return EXIT_FAILURE;
  }
  catch (...)
  {
    spdlog::critical("Unknown exception");
    /// @todo use https://en.cppreference.com/w/cpp/utility/basic_stacktrace
    logFailure();
    return EXIT_FAILURE;
  }

  spdlog::debug("------------------- End session (success) -------------------");
  return EXIT_SUCCESS;
}
