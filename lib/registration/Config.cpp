#include "registration/Config.h"

namespace registration
{
namespace
{

std::string pathString(const std::filesystem::path& path)
{
  return path.generic_string();
}

std::filesystem::path siblingExecutableIfDefault(
  const std::filesystem::path& configured,
  const std::filesystem::path& sibling,
  const char* defaultExecutableName)
{
  if (configured != defaultExecutableName || sibling.empty() || !sibling.has_parent_path()) {
    return configured;
  }
  return sibling.parent_path() / defaultExecutableName;
}

} // namespace

CommandGenerationOptions commandOptions(const BackendConfig& config)
{
  CommandGenerationOptions options;
  options.greedyExecutable = pathString(config.greedyExecutable);
  options.antsRegistrationExecutable = pathString(config.antsRegistrationExecutable);
  options.antsApplyTransformsExecutable = pathString(config.antsApplyTransformsExecutable);
  options.antsConvertTransformFileExecutable = siblingExecutableIfDefault(
                                                 config.antsConvertTransformFileExecutable,
                                                 config.antsRegistrationExecutable,
                                                 "ConvertTransformFile")
                                                 .generic_string();
  options.fireAntsPythonExecutable = pathString(config.fireAntsPythonExecutable);
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
