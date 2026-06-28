#include "registration/Availability.h"

#include <catch2/catch_test_macros.hpp>

#include <utility>

namespace
{

class FakeProbe final : public registration::IExecutableProbe
{
public:
  registration::ExecutableProbeResult result;
  std::filesystem::path executable;
  std::vector<std::string> arguments;

  registration::ExecutableProbeResult probe(
    const std::filesystem::path& executablePath,
    const std::vector<std::string>& probeArguments) override
  {
    executable = executablePath;
    arguments = probeArguments;
    return result;
  }
};

} // namespace

TEST_CASE("registration backend availability checks Greedy version command", "[registration][availability]")
{
  registration::BackendConfig config;
  config.greedyExecutable = "/opt/greedy";

  FakeProbe probe;
  probe.result.found = true;
  probe.result.exitCode = 0;
  probe.result.standardOutput = "greedy version 1.0";

  const registration::BackendAvailability availability =
    registration::checkBackendAvailability(registration::Backend::Greedy, config, probe);

  CHECK(probe.executable == "/opt/greedy");
  REQUIRE(probe.arguments.size() == 1);
  CHECK(probe.arguments.front() == "-version");
  CHECK(availability.status == registration::BackendAvailabilityStatus::Available);
  CHECK(availability.versionText == "greedy version 1.0");
}

TEST_CASE("registration backend availability checks FireANTs bridge module", "[registration][availability]")
{
  registration::BackendConfig config;
  config.fireAntsPythonExecutable = "/venv/bin/python";
  config.fireAntsBridgeModule = "custom_bridge";

  FakeProbe probe;
  probe.result.found = true;
  probe.result.exitCode = 0;

  const registration::BackendAvailability availability =
    registration::checkBackendAvailability(registration::Backend::FireANTs, config, probe);

  CHECK(probe.executable == "/venv/bin/python");
  REQUIRE(probe.arguments.size() == 3);
  CHECK(probe.arguments.at(0) == "-m");
  CHECK(probe.arguments.at(1) == "custom_bridge");
  CHECK(probe.arguments.at(2) == "check");
  CHECK(availability.status == registration::BackendAvailabilityStatus::Available);
}

TEST_CASE("registration backend availability reports missing executables", "[registration][availability]")
{
  registration::BackendConfig config;
  config.antsRegistrationExecutable = "/missing/antsRegistration";

  FakeProbe probe;
  probe.result.found = false;
  probe.result.failureMessage = "not found";

  const registration::BackendAvailability availability =
    registration::checkBackendAvailability(registration::Backend::ANTs, config, probe);

  CHECK(availability.status == registration::BackendAvailabilityStatus::Missing);
  CHECK(availability.message == "not found");
}

TEST_CASE("registration backend availability reports executable errors", "[registration][availability]")
{
  registration::BackendConfig config;

  FakeProbe probe;
  probe.result.found = true;
  probe.result.exitCode = 2;
  probe.result.standardError = "cuda unavailable";

  const registration::BackendAvailability availability =
    registration::checkBackendAvailability(registration::Backend::FireANTs, config, probe);

  CHECK(availability.status == registration::BackendAvailabilityStatus::Error);
  CHECK(availability.message == "cuda unavailable");
}
