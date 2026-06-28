#include "registration/Availability.h"

#include <algorithm>

namespace registration
{
namespace
{

std::string firstNonEmpty(std::initializer_list<std::string> values)
{
  const auto it = std::find_if(values.begin(), values.end(), [](const std::string& value) { return !value.empty(); });
  return it == values.end() ? std::string{} : *it;
}

std::string availabilitySuccessMessage(Backend backend)
{
  return std::string(label(backend)) + " is available.";
}

std::string availabilityMissingMessage(Backend backend, const std::filesystem::path& executable)
{
  return std::string(label(backend)) + " executable was not found: " + executable.string();
}

std::string availabilityErrorMessage(Backend backend)
{
  return std::string(label(backend)) + " responded with an error.";
}

} // namespace

std::vector<std::string> probeArguments(Backend backend, const BackendConfig& config)
{
  switch (backend) {
    case Backend::Greedy:
      return {"-version"};
    case Backend::ANTs:
      return {"--version"};
    case Backend::FireANTs:
      return {"-m", config.fireAntsBridgeModule, "check"};
  }
  return {};
}

BackendAvailability checkBackendAvailability(Backend backend, const BackendConfig& config, IExecutableProbe& probe)
{
  BackendAvailability availability;
  availability.backend = backend;
  availability.executable = executableForBackend(config, backend);

  const ExecutableProbeResult result = probe.probe(availability.executable, probeArguments(backend, config));
  availability.versionText = firstNonEmpty({result.standardOutput, result.standardError});

  if (!result.found) {
    availability.status = BackendAvailabilityStatus::Missing;
    availability.message = result.failureMessage.empty() ? availabilityMissingMessage(backend, availability.executable)
                                                         : result.failureMessage;
    return availability;
  }

  if (result.exitCode != 0) {
    availability.status = BackendAvailabilityStatus::Error;
    availability.message =
      firstNonEmpty({result.failureMessage, result.standardError, availabilityErrorMessage(backend)});
    return availability;
  }

  availability.status = BackendAvailabilityStatus::Available;
  availability.message = availabilitySuccessMessage(backend);
  return availability;
}

} // namespace registration
