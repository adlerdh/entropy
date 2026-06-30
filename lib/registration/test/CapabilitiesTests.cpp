#include "registration/Capabilities.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <string_view>

namespace
{

const registration::ParameterSchema* findParameter(
  const registration::BackendCapabilities& capabilities,
  std::string_view key)
{
  const auto it =
    std::find_if(capabilities.parameters.begin(), capabilities.parameters.end(), [&](const auto& parameter) {
      return parameter.key == key;
    });
  return it == capabilities.parameters.end() ? nullptr : &*it;
}

} // namespace

TEST_CASE("registration backend capabilities expose expected high-level features", "[registration]")
{
  const registration::BackendCapabilities greedy = registration::capabilitiesForBackend(registration::Backend::Greedy);
  CHECK(registration::supportsFeature(greedy, registration::Feature::SurfaceTransform));
  CHECK(registration::supportsFeature(greedy, registration::Feature::WarpedSegmentationOutput));
  CHECK_FALSE(registration::supportsFeature(greedy, registration::Feature::LandmarkDrivenRegistration));
  CHECK(registration::supportsMetric(greedy, registration::Metric::WNCC));
  CHECK(registration::supportsTransformModel(greedy, registration::TransformModel::Rigid));
  CHECK(registration::supportsTransformModel(greedy, registration::TransformModel::Similarity));
  CHECK(registration::supportsTransformModel(greedy, registration::TransformModel::Affine));
  CHECK_FALSE(registration::supportsTransformModel(greedy, registration::TransformModel::RigidAffine));
  const registration::ParameterSchema* greedyVerbosity = findParameter(greedy, "verbosity");
  REQUIRE(greedyVerbosity);
  CHECK(greedyVerbosity->advanced);
  CHECK(greedyVerbosity->defaultValue == "Default (1)");

  const registration::BackendCapabilities ants = registration::capabilitiesForBackend(registration::Backend::ANTs);
  CHECK_FALSE(registration::supportsFeature(ants, registration::Feature::LandmarkDrivenRegistration));
  CHECK_FALSE(registration::supportsMetric(ants, registration::Metric::PointSet));
  CHECK(registration::supportsMetric(ants, registration::Metric::MI));
  CHECK(registration::supportsTransformModel(ants, registration::TransformModel::TimeVaryingVelocity));
  CHECK_FALSE(registration::supportsTransformModel(ants, registration::TransformModel::RigidAffine));

  const registration::BackendCapabilities fireAnts =
    registration::capabilitiesForBackend(registration::Backend::FireANTs);
  CHECK(registration::supportsFeature(fireAnts, registration::Feature::StructuredProgress));
  CHECK(registration::supportsMetric(fireAnts, registration::Metric::CC));
  CHECK_FALSE(registration::supportsMetric(fireAnts, registration::Metric::MaskedCC));
  CHECK_FALSE(registration::supportsFeature(fireAnts, registration::Feature::SurfaceTransform));
  const registration::ParameterSchema* fireAntsVerbose = findParameter(fireAnts, "verbose");
  REQUIRE(fireAntsVerbose);
  CHECK(fireAntsVerbose->advanced);
  CHECK(fireAntsVerbose->defaultValue == "true");
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

TEST_CASE("registration backends use the shared coarse-to-fine iteration default", "[registration]")
{
  for (const registration::Backend backend :
       {registration::Backend::Greedy, registration::Backend::ANTs, registration::Backend::FireANTs})
  {
    const registration::BackendCapabilities capabilities = registration::capabilitiesForBackend(backend);
    const registration::ParameterSchema* iterations = findParameter(capabilities, "iterations");
    REQUIRE(iterations);
    CHECK(iterations->label == "Iterations per level (coarse to fine):");
    CHECK(iterations->defaultValue == "128x64x32");
  }
}
