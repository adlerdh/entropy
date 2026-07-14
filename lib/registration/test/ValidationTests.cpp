#include "registration/Capabilities.h"
#include "registration/Validation.h"

#include <catch2/catch_test_macros.hpp>

namespace
{

registration::DataRef imageRef(std::string uid, std::string name)
{
  registration::DataRef ref;
  ref.uid = std::move(uid);
  ref.displayName = std::move(name);
  ref.fileName = ref.displayName + ".nii.gz";
  ref.source = registration::DataSource::LoadedImage;
  return ref;
}

registration::JobSpec minimalJob(registration::Backend backend)
{
  registration::JobSpec job;
  job.backend = backend;
  job.fixedImage = imageRef("fixed", "fixed");
  job.movingImage = imageRef("moving", "moving");
  if (backend == registration::Backend::FireANTs) {
    job.metric = registration::Metric::MI;
    job.outputs.loadForwardWarp = false;
    job.outputs.transformLandmarksAndAnnotations = false;
  }
  return job;
}

} // namespace

TEST_CASE("registration validation accepts a minimal Greedy job", "[registration]")
{
  const registration::JobSpec job = minimalJob(registration::Backend::Greedy);
  const registration::ValidationResult result =
    registration::validateJob(job, registration::capabilitiesForBackend(job.backend));

  CHECK(result.canLaunch());
}

TEST_CASE("registration validation rejects same fixed and moving image", "[registration]")
{
  registration::JobSpec job = minimalJob(registration::Backend::Greedy);
  job.movingImage.uid = job.fixedImage.uid;

  const registration::ValidationResult result =
    registration::validateJob(job, registration::capabilitiesForBackend(job.backend));

  CHECK_FALSE(result.canLaunch());
}

TEST_CASE("registration validation reports unsupported backend features", "[registration]")
{
  registration::JobSpec job = minimalJob(registration::Backend::FireANTs);
  job.surfaces.push_back(registration::DataRef{"surface", "surface.vtk", "surface", registration::DataSource::Surface});

  const registration::ValidationResult result =
    registration::validateJob(job, registration::capabilitiesForBackend(job.backend));

  CHECK_FALSE(result.canLaunch());
}

TEST_CASE("registration validation warns when landmarks cannot drive backend optimization", "[registration]")
{
  registration::JobSpec job = minimalJob(registration::Backend::Greedy);
  job.landmarks.enabled = true;
  job.landmarks.matchedPairs = 4;

  const registration::ValidationResult result =
    registration::validateJob(job, registration::capabilitiesForBackend(job.backend));

  CHECK(result.canLaunch());
  REQUIRE_FALSE(result.messages.empty());
  CHECK(result.messages.front().severity == registration::ValidationSeverity::Warning);
}

TEST_CASE("registration validation requires a moving segmentation for warped segmentation output", "[registration]")
{
  registration::JobSpec job = minimalJob(registration::Backend::Greedy);
  job.outputs.loadWarpedSegmentation = true;

  registration::ValidationResult result =
    registration::validateJob(job, registration::capabilitiesForBackend(job.backend));

  CHECK_FALSE(result.canLaunch());

  job.movingMask = imageRef("moving-seg", "moving_seg");
  result = registration::validateJob(job, registration::capabilitiesForBackend(job.backend));

  CHECK(result.canLaunch());
}

TEST_CASE("registration validation allows FireANTs masks for documented masked metrics", "[registration]")
{
  registration::JobSpec job = minimalJob(registration::Backend::FireANTs);
  job.metric = registration::Metric::CC;
  job.fixedMask = imageRef("fixed-mask", "fixed_mask");

  registration::ValidationResult result =
    registration::validateJob(job, registration::capabilitiesForBackend(job.backend));

  CHECK(result.canLaunch());

  job.metric = registration::Metric::MSE;
  job.movingMask = imageRef("moving-mask", "moving_mask");
  result = registration::validateJob(job, registration::capabilitiesForBackend(job.backend));

  CHECK(result.canLaunch());
}

TEST_CASE("registration validation rejects FireANTs masks with undocumented masked metrics", "[registration]")
{
  registration::JobSpec job = minimalJob(registration::Backend::FireANTs);
  job.metric = registration::Metric::MI;
  job.fixedMask = imageRef("fixed-mask", "fixed_mask");

  const registration::ValidationResult result =
    registration::validateJob(job, registration::capabilitiesForBackend(job.backend));

  CHECK_FALSE(result.canLaunch());
}

TEST_CASE("registration validation rejects FireANTs initial affine files", "[registration]")
{
  registration::JobSpec job = minimalJob(registration::Backend::FireANTs);
  job.initialAffineTransform = "/tmp/entropy-registration/initial_affine.tfm";

  const registration::ValidationResult result =
    registration::validateJob(job, registration::capabilitiesForBackend(job.backend));

  CHECK_FALSE(result.canLaunch());
}

TEST_CASE("registration validation rejects FireANTs metrics not supported by the CLI", "[registration]")
{
  registration::JobSpec job = minimalJob(registration::Backend::FireANTs);
  job.metric = registration::Metric::WNCC;

  registration::ValidationResult result =
    registration::validateJob(job, registration::capabilitiesForBackend(job.backend));

  CHECK_FALSE(result.canLaunch());

  job.metric = registration::Metric::NCC;
  result = registration::validateJob(job, registration::capabilitiesForBackend(job.backend));

  CHECK_FALSE(result.canLaunch());

  job.metric = registration::Metric::CC;
  result = registration::validateJob(job, registration::capabilitiesForBackend(job.backend));

  CHECK(result.canLaunch());
}

TEST_CASE("registration validation rejects FireANTs Levenberg-Marquardt with geodesic deformation", "[registration]")
{
  registration::JobSpec job = minimalJob(registration::Backend::FireANTs);
  job.transformModel = registration::TransformModel::Deformable;
  job.parameterValues.push_back({"deformableOptimizer", "Levenberg-Marquardt"});
  job.parameterValues.push_back({"deformationModel", "geodesic"});

  registration::ValidationResult result =
    registration::validateJob(job, registration::capabilitiesForBackend(job.backend));

  CHECK_FALSE(result.canLaunch());

  job.parameterValues.back().value = "compositive";
  result = registration::validateJob(job, registration::capabilitiesForBackend(job.backend));

  CHECK(result.canLaunch());
}

TEST_CASE("registration validation warns about FireANTs missing staged affine output", "[registration]")
{
  registration::JobSpec job = minimalJob(registration::Backend::FireANTs);
  job.transformModel = registration::TransformModel::AffineDeformable;
  job.outputs.loadAffineTransform = true;

  registration::ValidationResult result =
    registration::validateJob(job, registration::capabilitiesForBackend(job.backend));

  CHECK(result.canLaunch());
  REQUIRE_FALSE(result.messages.empty());
  CHECK(result.messages.front().severity == registration::ValidationSeverity::Warning);
  CHECK(result.messages.front().field == "outputs.affineTransform");

  job.outputs.loadAffineTransform = false;
  result = registration::validateJob(job, registration::capabilitiesForBackend(job.backend));

  CHECK(result.canLaunch());
  CHECK(
    std::none_of(result.messages.begin(), result.messages.end(), [](const registration::ValidationMessage& message) {
      return message.field == "outputs.affineTransform";
    }));
}

TEST_CASE("registration validation warns when Greedy affine-only jobs include a moving mask", "[registration]")
{
  registration::JobSpec job = minimalJob(registration::Backend::Greedy);
  job.transformModel = registration::TransformModel::Affine;
  job.movingMask = imageRef("moving-mask", "moving_mask");
  job.outputPrefix = "moving_to_fixed";

  registration::ValidationResult result =
    registration::validateJob(job, registration::capabilitiesForBackend(job.backend));

  CHECK(result.canLaunch());
  REQUIRE_FALSE(result.messages.empty());
  CHECK(result.messages.front().severity == registration::ValidationSeverity::Warning);

  job.transformModel = registration::TransformModel::AffineDeformable;
  result = registration::validateJob(job, registration::capabilitiesForBackend(job.backend));

  CHECK(result.canLaunch());
  CHECK(result.messages.empty());
}

TEST_CASE("registration validation rejects incomplete auxiliary image pairs", "[registration]")
{
  registration::JobSpec job = minimalJob(registration::Backend::Greedy);
  registration::AuxiliaryImagePair pair;
  pair.fixed = imageRef("aux-fixed", "aux_fixed");
  pair.weight = 1.0;
  job.auxiliaryImagePairs.push_back(pair);

  registration::ValidationResult result =
    registration::validateJob(job, registration::capabilitiesForBackend(job.backend));

  CHECK_FALSE(result.canLaunch());

  job.auxiliaryImagePairs.front().moving = imageRef("aux-moving", "aux_moving");
  result = registration::validateJob(job, registration::capabilitiesForBackend(job.backend));

  CHECK(result.canLaunch());
}
