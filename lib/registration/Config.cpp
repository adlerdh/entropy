#include "registration/Config.h"

namespace registration
{

CommandGenerationOptions commandOptions(const BackendConfig& config)
{
  CommandGenerationOptions options;
  options.greedyExecutable = config.greedyExecutable.string();
  options.antsRegistrationExecutable = config.antsRegistrationExecutable.string();
  options.fireAntsPythonExecutable = config.fireAntsPythonExecutable.string();
  options.fireAntsBridgeModule = config.fireAntsBridgeModule;
  return options;
}

std::filesystem::path executableForBackend(const BackendConfig& config, Backend backend)
{
  switch (backend) {
    case Backend::Greedy:
      return config.greedyExecutable;
    case Backend::ANTs:
      return config.antsRegistrationExecutable;
    case Backend::FireANTs:
      return config.fireAntsPythonExecutable;
  }
  return {};
}

} // namespace registration
