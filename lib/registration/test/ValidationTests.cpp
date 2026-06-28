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
