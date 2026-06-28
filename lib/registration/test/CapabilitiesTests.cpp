#include "registration/Capabilities.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("registration backend capabilities expose expected high-level features", "[registration]")
{
  const registration::BackendCapabilities greedy = registration::capabilitiesForBackend(registration::Backend::Greedy);
  CHECK(registration::supportsFeature(greedy, registration::Feature::SurfaceTransform));
  CHECK(registration::supportsFeature(greedy, registration::Feature::WarpedSegmentationOutput));
  CHECK_FALSE(registration::supportsFeature(greedy, registration::Feature::LandmarkDrivenRegistration));
  CHECK(registration::supportsMetric(greedy, registration::Metric::WNCC));

  const registration::BackendCapabilities ants = registration::capabilitiesForBackend(registration::Backend::ANTs);
  CHECK(registration::supportsFeature(ants, registration::Feature::LandmarkDrivenRegistration));
  CHECK(registration::supportsMetric(ants, registration::Metric::PointSet));
  CHECK(registration::supportsTransformModel(ants, registration::TransformModel::TimeVaryingVelocity));

  const registration::BackendCapabilities fireAnts =
    registration::capabilitiesForBackend(registration::Backend::FireANTs);
  CHECK(registration::supportsFeature(fireAnts, registration::Feature::StructuredProgress));
  CHECK(registration::supportsMetric(fireAnts, registration::Metric::MaskedCC));
  CHECK_FALSE(registration::supportsFeature(fireAnts, registration::Feature::SurfaceTransform));
}

TEST_CASE("registration backend schemas include user-facing tooltip text", "[registration]")
{
  for (const registration::Backend backend :
       {registration::Backend::Greedy, registration::Backend::ANTs, registration::Backend::FireANTs})
  {
    const registration::BackendCapabilities capabilities = registration::capabilitiesForBackend(backend);
    REQUIRE_FALSE(capabilities.parameters.empty());
    for (const registration::ParameterSchema& parameter : capabilities.parameters) {
      CHECK_FALSE(parameter.key.empty());
      CHECK_FALSE(parameter.label.empty());
      CHECK_FALSE(parameter.tooltip.empty());
    }
  }
}
