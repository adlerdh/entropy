#include "registration/Capabilities.h"
#include "registration/Json.h"

#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

namespace
{

registration::DataRef makeRef(std::string uid, std::string fileName, registration::DataSource source)
{
  registration::DataRef ref;
  ref.uid = std::move(uid);
  ref.fileName = std::move(fileName);
  ref.displayName = ref.uid;
  ref.source = source;
  return ref;
}

registration::JobSpec makeJob()
{
  registration::JobSpec job;
  job.backend = registration::Backend::ANTs;
  job.fixedImage = makeRef("fixed", "fixed.nii.gz", registration::DataSource::LoadedImage);
  job.movingImage = makeRef("moving", "moving.nii.gz", registration::DataSource::LoadedImage);
  job.transformModel = registration::TransformModel::AffineDeformable;
  job.metric = registration::Metric::CC;
  job.fixedMask = makeRef("fixed-mask", "fixed-mask.nii.gz", registration::DataSource::Segmentation);
  job.outputDirectory = "/tmp/entropy-registration";
  job.outputPrefix = "moving_to_fixed";
  job.outputs.loadWarpedSegmentation = true;
  job.landmarks.enabled = true;
  job.landmarks.matchedPairs = 5;
  job.landmarks.fixedLandmarks = makeRef("fixed-lm", "fixed.csv", registration::DataSource::LandmarkGroup);
  job.landmarks.movingLandmarks = makeRef("moving-lm", "moving.csv", registration::DataSource::LandmarkGroup);
  job.parameterValues = {{"iterations", "40x20"}, {"threads", "6"}};
  job.extraArguments = {"--verbose", "1"};
  return job;
}

} // namespace

TEST_CASE("registration job specs round-trip through JSON", "[registration][serialization]")
{
  const registration::JobSpec original = makeJob();

  const nlohmann::json json = original;
  const registration::JobSpec restored = json.get<registration::JobSpec>();

  CHECK(json.at("backend") == "ANTs");
  CHECK(json.at("transformModel") == "AffineDeformable");
  CHECK(json.at("metric") == "CC");
  CHECK(restored.backend == original.backend);
  CHECK(restored.fixedImage.uid == original.fixedImage.uid);
  CHECK(restored.movingImage.fileName == original.movingImage.fileName);
  CHECK(restored.fixedMask.source == registration::DataSource::Segmentation);
  CHECK(restored.outputs.loadWarpedSegmentation);
  CHECK(restored.landmarks.enabled);
  CHECK(restored.landmarks.matchedPairs == 5);
  REQUIRE(restored.parameterValues.size() == 2);
  CHECK(restored.parameterValues.front().key == "iterations");
  CHECK(restored.parameterValues.front().value == "40x20");
  CHECK(restored.extraArguments == original.extraArguments);
}

TEST_CASE("registration capabilities serialize readable enum names", "[registration][serialization]")
{
  const registration::BackendCapabilities capabilities =
    registration::capabilitiesForBackend(registration::Backend::FireANTs);

  const nlohmann::json json = capabilities;

  CHECK(json.at("backend") == "FireANTs");
  CHECK(json.at("features").at(0).is_string());
  CHECK(json.at("parameters").at(0).at("tooltip").is_string());

  const registration::BackendCapabilities restored = json.get<registration::BackendCapabilities>();
  CHECK(restored.backend == registration::Backend::FireANTs);
  CHECK(registration::supportsMetric(restored, registration::Metric::MaskedCC));
}

TEST_CASE("registration progress events round-trip through JSON", "[registration][serialization]")
{
  registration::ProgressEvent event;
  event.kind = registration::ProgressEventKind::Progress;
  event.jobId = "job";
  event.stageName = "deformable";
  event.iteration = 3;
  event.iterations = 10;
  event.progress = 0.3;
  event.loss = 0.125;

  const nlohmann::json json = event;
  const registration::ProgressEvent restored = json.get<registration::ProgressEvent>();

  CHECK(json.at("event") == "Progress");
  REQUIRE(restored.progress);
  CHECK(*restored.progress == 0.3);
  REQUIRE(restored.loss);
  CHECK(*restored.loss == 0.125);
}

TEST_CASE("registration result manifests round-trip artifact paths", "[registration][serialization]")
{
  registration::ResultManifest manifest;
  manifest.success = true;
  manifest.backend = registration::Backend::Greedy;
  manifest.fixedImageUid = "fixed";
  manifest.movingImageUid = "moving";
  manifest.warpedImage = "warped.nii.gz";
  manifest.inverseWarp = "inverse.nii.gz";
  manifest.forwardWarp = "forward.nii.gz";
  manifest.transformedSurfaces = {"surface.vtk"};
  manifest.warnings = {"warning"};
  manifest.warpConvention = "fixed_to_moving_sampling_displacement_lps_mm";

  const nlohmann::json json = manifest;
  const registration::ResultManifest restored = json.get<registration::ResultManifest>();

  CHECK(json.at("backend") == "Greedy");
  CHECK(restored.success);
  CHECK(restored.inverseWarp == manifest.inverseWarp);
  REQUIRE(restored.transformedSurfaces.size() == 1);
  CHECK(restored.transformedSurfaces.front() == "surface.vtk");
  CHECK(restored.warpConvention == manifest.warpConvention);
}

TEST_CASE("registration backend config round-trips through JSON", "[registration][serialization]")
{
  registration::BackendConfig config;
  config.defaultBackend = registration::Backend::FireANTs;
  config.greedyExecutable = "/opt/greedy";
  config.antsRegistrationExecutable = "/opt/antsRegistration";
  config.antsApplyTransformsExecutable = "/opt/antsApplyTransforms";
  config.fireAntsPythonExecutable = "/venv/bin/python";
  config.fireAntsBridgeModule = "custom_bridge";
  config.defaultOutputDirectory = "/tmp/entropy-registration";
  config.keepTemporaryFiles = true;
  config.maxConcurrentJobs = 2;
  config.defaultCpuThreadCount = 8;
  config.defaultFireAntsDevice = "cuda:1";
  config.showExpertOptionsByDefault = true;

  const nlohmann::json json = config;
  const registration::BackendConfig restored = json.get<registration::BackendConfig>();

  CHECK(json.at("defaultBackend") == "FireANTs");
  CHECK(restored.defaultBackend == registration::Backend::FireANTs);
  CHECK(restored.greedyExecutable == "/opt/greedy");
  CHECK(restored.antsRegistrationExecutable == "/opt/antsRegistration");
  CHECK(restored.antsApplyTransformsExecutable == "/opt/antsApplyTransforms");
  CHECK(restored.fireAntsPythonExecutable == "/venv/bin/python");
  CHECK(restored.fireAntsBridgeModule == "custom_bridge");
  CHECK(restored.defaultOutputDirectory == "/tmp/entropy-registration");
  CHECK(restored.keepTemporaryFiles);
  CHECK(restored.maxConcurrentJobs == 2);
  CHECK(restored.defaultCpuThreadCount == 8);
  CHECK(restored.defaultFireAntsDevice == "cuda:1");
  CHECK(restored.showExpertOptionsByDefault);
}
