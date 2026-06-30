#include "registration/Config.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("registration backend config converts to command options", "[registration][config]")
{
  registration::BackendConfig config;
  config.greedyExecutable = "/opt/greedy";
  config.antsRegistrationExecutable = "/opt/antsRegistration";
  config.antsApplyTransformsExecutable = "/opt/antsApplyTransforms";
  config.antsConvertTransformFileExecutable = "/opt/ConvertTransformFile";
  config.fireAntsPythonExecutable = "/opt/python";
  config.fireAntsBridgeModule = "entropy_custom_fireants";

  const registration::CommandGenerationOptions options = registration::commandOptions(config);

  CHECK(options.greedyExecutable == "/opt/greedy");
  CHECK(options.antsRegistrationExecutable == "/opt/antsRegistration");
  CHECK(options.antsApplyTransformsExecutable == "/opt/antsApplyTransforms");
  CHECK(options.antsConvertTransformFileExecutable == "/opt/ConvertTransformFile");
  CHECK(options.fireAntsPythonExecutable == "/opt/python");
  CHECK(options.fireAntsBridgeModule == "entropy_custom_fireants");
}

TEST_CASE("registration backend config returns executable by backend", "[registration][config]")
{
  registration::BackendConfig config;
  config.greedyExecutable = "greedy-custom";
  config.antsRegistrationExecutable = "ants-custom";
  config.fireAntsPythonExecutable = "python-custom";

  CHECK(registration::executableForBackend(config, registration::Backend::Greedy) == "greedy-custom");
  CHECK(registration::executableForBackend(config, registration::Backend::ANTs) == "ants-custom");
  CHECK(registration::executableForBackend(config, registration::Backend::FireANTs) == "python-custom");
}

TEST_CASE(
  "registration backend config derives ANTs helper executable beside antsRegistration",
  "[registration][config]")
{
  registration::BackendConfig config;
  config.antsRegistrationExecutable = "/opt/ants/bin/antsRegistration";

  const registration::CommandGenerationOptions options = registration::commandOptions(config);

  CHECK(options.antsConvertTransformFileExecutable == "/opt/ants/bin/ConvertTransformFile");
}
