#include "registration/Availability.h"

#include <algorithm>
#include <regex>
#include <sstream>

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

std::string versionString(const SemanticVersion& version)
{
  std::ostringstream stream;
  stream << version.major << '.' << version.minor << '.' << version.patch;
  return stream.str();
}

bool versionLessThan(const SemanticVersion& lhs, const SemanticVersion& rhs)
{
  if (lhs.major != rhs.major) {
    return lhs.major < rhs.major;
  }
  if (lhs.minor != rhs.minor) {
    return lhs.minor < rhs.minor;
  }
  return lhs.patch < rhs.patch;
}

void assessCompatibility(BackendAvailability& availability)
{
  const std::optional<SemanticVersion> minimumVersion = minimumSupportedVersion(availability.backend);
  if (!minimumVersion) {
    availability.compatibility = BackendCompatibilityStatus::Untested;
    availability.compatibilityMessage =
      std::string(label(availability.backend)) + " is available, but Entropy does not yet enforce a version range.";
    return;
  }

  if (!availability.detectedVersion) {
    availability.compatibility = BackendCompatibilityStatus::Untested;
    availability.compatibilityMessage = std::string(label(availability.backend)) +
                                        " is available, but Entropy could not parse its version. "
                                        "Expected version >= " +
                                        versionString(*minimumVersion) + '.';
    return;
  }

  if (versionLessThan(*availability.detectedVersion, *minimumVersion)) {
    availability.compatibility = BackendCompatibilityStatus::Incompatible;
    availability.compatibilityMessage =
      std::string(label(availability.backend)) + " " + versionString(*availability.detectedVersion) +
      " is older than Entropy's supported minimum " + versionString(*minimumVersion) + '.';
    return;
  }

  availability.compatibility = BackendCompatibilityStatus::Compatible;
  availability.compatibilityMessage =
    std::string(label(availability.backend)) + " " + versionString(*availability.detectedVersion) +
    " satisfies Entropy's minimum supported version " + versionString(*minimumVersion) + '.';
}

} // namespace

std::optional<SemanticVersion> minimumSupportedVersion(Backend backend)
{
  switch (backend) {
    case Backend::Greedy:
      return SemanticVersion{1, 0, 0};
    case Backend::ANTs:
      return SemanticVersion{2, 5, 0};
    case Backend::FireANTs:
      return std::nullopt;
  }
  return std::nullopt;
}

std::optional<SemanticVersion> parseSemanticVersion(std::string_view text)
{
  static const std::regex versionPattern{R"((\d+)(?:\.(\d+))?(?:\.(\d+))?)"};
  const std::string value{text};
  std::smatch match;
  if (!std::regex_search(value, match, versionPattern)) {
    return std::nullopt;
  }

  SemanticVersion version;
  version.major = std::stoi(match[1].str());
  if (match[2].matched) {
    version.minor = std::stoi(match[2].str());
  }
  if (match[3].matched) {
    version.patch = std::stoi(match[3].str());
  }
  return version;
}

std::vector<std::string> probeArguments(Backend backend, const BackendConfig&)
{
  switch (backend) {
    case Backend::Greedy:
      return {"-version"};
    case Backend::ANTs:
      return {"--version"};
    case Backend::FireANTs:
      return {"-m", "fireants_bridge", "check"};
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
  availability.detectedVersion = parseSemanticVersion(availability.versionText);

  if (!result.found) {
    availability.status = BackendAvailabilityStatus::Missing;
    availability.compatibility = BackendCompatibilityStatus::Untested;
    availability.message = result.failureMessage.empty() ? availabilityMissingMessage(backend, availability.executable)
                                                         : result.failureMessage;
    return availability;
  }

  if (result.exitCode != 0) {
    availability.status = BackendAvailabilityStatus::Error;
    availability.compatibility = BackendCompatibilityStatus::Untested;
    availability.message =
      firstNonEmpty({result.failureMessage, result.standardError, availabilityErrorMessage(backend)});
    return availability;
  }

  availability.status = BackendAvailabilityStatus::Available;
  assessCompatibility(availability);
  availability.message = availabilitySuccessMessage(backend);
  if (availability.compatibility == BackendCompatibilityStatus::Incompatible) {
    availability.message += ' ';
    availability.message += availability.compatibilityMessage;
  }
  return availability;
}

} // namespace registration
