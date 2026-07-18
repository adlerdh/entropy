#pragma once

#include "registration/Config.h"
#include "registration/Types.h"

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace registration
{

/**
 * @brief Availability state for an external registration backend.
 */
enum class BackendAvailabilityStatus
{
  Available, //!< Executable exists and reported successfully
  Missing,   //!< Executable could not be found or launched
  Error      //!< Executable launched but reported an error
};

/**
 * @brief Compatibility assessment for an available backend.
 */
enum class BackendCompatibilityStatus
{
  Compatible,  //!< Backend version is known to satisfy Entropy's requirements
  Untested,    //!< Backend is available, but its version could not be validated
  Incompatible //!< Backend version is older than Entropy's supported minimum
};

/**
 * @brief Semantic version parsed from backend probe output.
 */
struct SemanticVersion
{
  int major = 0; //!< Major version number
  int minor = 0; //!< Minor version number
  int patch = 0; //!< Patch version number
};

/**
 * @brief Result of probing one executable.
 */
struct ExecutableProbeResult
{
  bool found = false;         //!< True when the executable exists or can be launched
  int exitCode = 0;           //!< Process exit code from the probe command
  std::string standardOutput; //!< Captured stdout text
  std::string standardError;  //!< Captured stderr text
  std::string failureMessage; //!< Launch or probe failure explanation
};

/**
 * @brief Abstract executable probe used to keep backend checks testable.
 */
class IExecutableProbe
{
public:
  virtual ~IExecutableProbe() = default;

  /**
   * @brief Probe one executable.
   * @param executable Executable path or command name.
   * @param arguments Arguments used for the probe.
   * @return Probe result.
   */
  virtual ExecutableProbeResult probe(
    const std::filesystem::path& executable,
    const std::vector<std::string>& arguments) = 0;
};

/**
 * @brief User-facing backend availability summary.
 */
struct BackendAvailability
{
  Backend backend = Backend::Greedy;                                     //!< Backend that was checked
  BackendAvailabilityStatus status = BackendAvailabilityStatus::Missing; //!< Availability status
  BackendCompatibilityStatus compatibility =                             //!< Version compatibility status
    BackendCompatibilityStatus::Untested;
  std::filesystem::path executable;               //!< Executable that was checked
  std::string versionText;                        //!< Version or identifying output
  std::optional<SemanticVersion> detectedVersion; //!< Parsed backend version, if available
  std::string message;                            //!< User-facing status message
  std::string compatibilityMessage;               //!< User-facing compatibility warning or note
};

/**
 * @brief Return Entropy's minimum supported backend version, if one is known.
 * @param backend Backend to query.
 * @return Minimum supported version, or nullopt when Entropy cannot enforce one.
 */
std::optional<SemanticVersion> minimumSupportedVersion(Backend backend);

/**
 * @brief Parse the first semantic version-like token from backend probe output.
 * @param text Raw backend probe output.
 * @return Parsed version, or nullopt if no version could be found.
 */
std::optional<SemanticVersion> parseSemanticVersion(std::string_view text);

/**
 * @brief Return the probe arguments for a backend.
 * @param backend Backend to probe.
 * @param config Backend configuration.
 * @return Arguments for a lightweight version/availability check.
 */
std::vector<std::string> probeArguments(Backend backend, const BackendConfig& config);

/**
 * @brief Check whether a backend is available.
 * @param backend Backend to check.
 * @param config Backend configuration.
 * @param probe Probe implementation.
 * @return Availability summary.
 */
BackendAvailability checkBackendAvailability(Backend backend, const BackendConfig& config, IExecutableProbe& probe);

} // namespace registration
